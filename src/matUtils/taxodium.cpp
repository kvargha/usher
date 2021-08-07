#include "taxodium.hpp"

enum Fields {genbank_col, date_col, country_col, host_col, completeness_col, length_col, clade_col, lineage_col};

// Helper function to format one attribute in attributes into taxodium encoding. Modifies most parameters
void populate_attribute(int attribute_column, std::vector<std::string> &attributes, std::unordered_map<std::string, std::string> &seen_map, int &encoding_counter, Taxodium::AllData &all_data) {
    if (seen_map.find(attributes[attribute_column]) == seen_map.end()) {
        encoding_counter++;
        std::string encoding_str = std::to_string(encoding_counter);
        seen_map[attributes[attribute_column]] = encoding_str;
        switch(attribute_column) {
            case country_col:
                all_data.add_country_mapping(attributes[attribute_column]);
                break;
            case date_col:
                all_data.add_date_mapping(attributes[attribute_column]);
                break;
            case lineage_col:
                all_data.add_lineage_mapping(attributes[attribute_column]);
                break;
        }
        attributes[attribute_column] = encoding_str;
    } else {
        attributes[attribute_column] = seen_map[attributes[attribute_column]];
    }
}

std::unordered_map<std::string, std::vector<std::string>> read_metafiles_tax(std::vector<std::string> filenames, Taxodium::AllData &all_data) {
    
    int32_t country_ct = 0;
    int32_t date_ct = 0;
    int32_t lineage_ct = 0;
    
    //The first index of theses lists in the pb indicates field is not present
    all_data.add_country_mapping("");
    all_data.add_date_mapping("");
    all_data.add_lineage_mapping("");

    // These three attributes are encoded as integers.
    // These maps map a country string to its integer representation
    // (as a string type so it fits in with other metadata)
    std::unordered_map<std::string, std::string> seen_countries_map; 
    std::unordered_map<std::string, std::string> seen_lineages_map;
    std::unordered_map<std::string, std::string> seen_dates_map;

    std::unordered_map<std::string, std::vector<std::string>> metadata;

    for (std::string f : filenames) {
        std::ifstream infile(f);
        if (!infile) {
            fprintf(stderr, "ERROR: Could not open the file: %s!\n", f.c_str());
            exit(1);
        }
        std::string line;
        char delim = '\t';
        if (f.find(".csv\0") != std::string::npos) {
            delim = ',';
        }
        while (std::getline(infile, line)) {
            std::vector<std::string> words;
            if (line[line.size()-1] == '\r') {
                line = line.substr(0, line.size()-1);
            }
            MAT::string_split(line, delim, words);
            std::string key = words[0];
            std::vector<std::string> attributes(&words[1], &words[words.size()]);
           
            if (attributes.size()-1 >= country_col) {
                populate_attribute(country_col, attributes, seen_countries_map, country_ct, all_data);
            }
            if (attributes.size()-1 >= date_col) {
                populate_attribute(date_col, attributes, seen_dates_map, date_ct, all_data);
            }
            if (attributes.size()-1 >= lineage_col) {
                populate_attribute(lineage_col, attributes, seen_lineages_map, lineage_ct, all_data);
            }
            metadata[key] = attributes;
        }
       infile.close();
     }
    return metadata;
}
void save_taxodium_tree (MAT::Tree &tree, std::string out_filename, std::vector<std::string> meta_filenames) {

	Taxodium::AllNodeData node_data;
	Taxodium::AllData all_data;

    std::unordered_map<std::string, std::vector<std::string>> metadata = read_metafiles_tax(meta_filenames, all_data);
	int count = 0;
	TIMEIT();

    auto dfs = tree.depth_first_expansion();

    for (size_t idx = 0; idx < dfs.size(); idx++) {
		if (count > 20) {
			break;
		}
		count++;
		MAT::Node *node = dfs[idx];
		
        node_data.add_names(node->identifier);

        if (node->identifier.substr(0,5) == "node_") {
            node_data.add_x(0);
            node_data.add_y(0);
            node_data.add_countries(0); 
            node_data.add_lineages(0); 
            node_data.add_dates(0); 
            //internal nodes don't have country, lineage, date            
        }

        if (metadata.find(node->identifier) != metadata.end()) {
            std::vector<std::string> meta_fields = metadata[node->identifier];
            int32_t country = std::stoi(meta_fields[country_col]);
            int32_t lineage = std::stoi(meta_fields[lineage_col]);
            int32_t date = std::stoi(meta_fields[date_col]);
            node_data.add_x(0);
            node_data.add_y(0);
            node_data.add_countries(country);
            node_data.add_lineages(lineage); 
            node_data.add_dates(date); 
        }
    }
    all_data.set_node_data(node_data);
    // Boost library used to stream the contents to the output protobuf file in
    // uncompressed or compressed .gz format
    std::ofstream outfile(out_filename, std::ios::out | std::ios::binary);
    boost::iostreams::filtering_streambuf< boost::iostreams::output> outbuf;

    if (out_filename.find(".gz\0") != std::string::npos) {
        try {
            outbuf.push(boost::iostreams::gzip_compressor());
            outbuf.push(outfile);
            std::ostream outstream(&outbuf);
            node_data.SerializeToOstream(&outstream);
            std::string s;
            google::protobuf::TextFormat::PrintToString(all_data, &s);
            std::cout << s << '\n';
            boost::iostreams::close(outbuf);
            outfile.close();
        } catch(const boost::iostreams::gzip_error& e) {
            std::cout << e.what() << '\n';
        }
    } else {
        node_data.SerializeToOstream(&outfile);
        std::string s;
        google::protobuf::TextFormat::PrintToString(all_data, &s);
        std::cout << s << '\n';
        outfile.close();
    }
}