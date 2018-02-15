#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <dirent.h>
#include <fstream>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <algorithm>

#define VERSION 1.2

const char* prog;

struct cmdOptions_t {
    const char* title; // the title tag of the index.html
    const char* charset; // the charset used in the meta tag
    const char* headline_files; // the headline of the files section
    const char* headline_directories; // the headline of the directories section
    const char* path_name; // the name of the path to index
    const char* list_type;
    DIR* path; // the PATH object for later work
    std::string outfile_name; // the name and location of the resulting index.html
    std::fstream outfile; // the actual object representing the outfile
    const char* exfile_name; // name of file containing a list of files to exclude from indexing
    std::fstream exfile; // the actual object representing the exfile
    bool include_index; // Include the index.html itself in the index file
    bool verbose; // determine whether or not to print information on what's going on
    bool sort; // set whether or not to sort the data

} cmdOptions;

static const char* optString = "t:l:c:f:d:p:o:e:isvhV";

struct work_data_t {
    std::vector<std::string> content;
    std::vector<std::string> files;
    std::vector<std::string> dirs;
    std::vector<std::string> exclude;
    std::string index_file;
} work_data;

struct option longOpts[] = {
        {"title", required_argument, NULL, 't'},
        {"charset", required_argument, NULL, 'c'},
        {"headline-files", required_argument, NULL, 'f'},
        {"headline-directories", required_argument, NULL, 'd'},
        {"path", required_argument, NULL, 'p'},
        {"outfile", required_argument, NULL, 'o'},
        {"exfile", required_argument, NULL, 'e'},
        {"list-type", required_argument, NULL, 'l'},
        {"sort", no_argument, NULL, 's'},
        {"verbose", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'V'},
        {"include-index", no_argument, NULL, 'i'}
};

void // print the progam's usage message and exit 
usage(int status) {
    std::cout << "usage: " << prog << " [options]\n";
    exit(status);
}

void
help() {
    std::cout << "usage: " << prog << " [options]\n\n"\
    "create an index html of the specified path (calling directory by default)\n" \
    "in order to have it simply available. Files that shall be excluded can be \n" \
    "specified in an additional file that is specified by the -e, --exfile option\n" \
    "Note: The exfile file will not be excluded as well, unless it is specified in it's own content!\n\n" \
    "options: \n" \
    "  -c, --charset CHARSET\tthe charset to use in the meta tag\n" \
    "  -d, --headline-directories HEADLINE the headline for the directories section\n" \
    "  -e, --exfile FILE\ta file containing a list files that will be excluded\n" \
    "  -f, --headline-files HEADLINE the headline for the files section\n" \
    "  -i, --include-index\tInclude the index file itself in the list of files\n" \
    "  -l, --list-type [ol, ul] the list type to use (default: ul) \n" \
    "  -o, --outfile FILE\tthe location of the resulting file (default: index.html)\n" \
    "  -p, --path PATH\tthe path to get the files from (default: current directory)\n" \
    "  -s, --sort\t\tsort the content of each, the directory and files section alphabetically\n" \
    "  -t, --title TITLE\tthe content of the title tag\n" \
    "  -v, --verbose\t\tprint what's currently going on\n" \
    "  -V, --version\t\tprint the program's version number and exit\n"\
    "  -h, --help\t\tprint this help message and exit\n";
    exit(0);
}

void 
version() {
    std::cout << VERSION << std::endl;
    exit(0);
}

void 
remove_exclude() {
    std::vector<std::string> fl = work_data.content;
    // filter excluded files
    for (int i = 0; i < fl.size(); i++) {
        for (int j = 0; j < work_data.exclude.size(); j++) {
            std::string s1 = fl[i];
            std::string s2 = work_data.exclude[j];
            if(s1 == s2) {
                work_data.content.erase(std::remove(work_data.content.begin(), work_data.content.end(), s1));
                work_data.exclude.erase(std::remove(work_data.exclude.begin(), work_data.exclude.end(), s2));
                continue;
            }
        }
    }
    if (cmdOptions.verbose) {
        if(!work_data.exclude.empty()) {
            std::cout << prog << ": info: not all files listed in '" << cmdOptions.exfile_name << "' exist\n";
        }
    }
}

void 
generate_file() {
    static std::string files_section = "";
    static std::string dirs_section = "";
    for (int i = 0; i < work_data.files.size(); i++) {
        files_section +=   "<li><a href=\"" + work_data.files[i] + "\" download>" + work_data.files[i] + \
        "</a></li>\n";
    }
    for (int i = 0; i < work_data.dirs.size(); i++) {
        dirs_section += "  <li><a href=\"" + work_data.dirs[i] + "\">" + work_data.dirs[i] + \
        "</a></li>\n";
    }

    std::string html_file = "<!DOCTYPE html>\n" \
    "<html>\n" \
    "<head>\n" \
    "  <title>" + (std::string)cmdOptions.title + "</title>\n" \
    "  <meta charset=\"" + (std::string) cmdOptions.charset + "\"/>\n" \
    "</head>\n" \
    "<body>\n" \
    "  <h1>" + (std::string) cmdOptions.headline_files + "</h1>\n" \
    "<" + (std::string) cmdOptions.list_type + ">\n" \
    + files_section + \
    "</" + (std::string) cmdOptions.list_type + ">\n" \
    "  <h1>" + (std::string) cmdOptions.headline_directories + "</h1>\n" \
    "<" + (std::string) cmdOptions.list_type + ">\n" \
    + dirs_section + "\n" \
    "</" + (std::string) cmdOptions.list_type + ">\n" \
    "</body>\n" \
    "</html>\n";

    if(cmdOptions.verbose) 
        std::cout << prog << ": writing generated file to " << cmdOptions.outfile_name << "\n";
    cmdOptions.outfile.write(html_file.c_str(), html_file.size());
}

void // compare stats of all files and sort them according to these stats 
sort_content() {
    for (int i = 0; i < work_data.content.size(); i++) {
        struct stat s;
        std::string object = (std::string) cmdOptions.path_name + "/" +work_data.content[i];
        if (stat(object.c_str(), &s) == 0) {
            if (S_ISDIR(s.st_mode)) 
                work_data.dirs.push_back(work_data.content[i]);
            else if (S_ISREG(s.st_mode))
                work_data.files.push_back(work_data.content[i]);
            else {
                if (cmdOptions.verbose) 
                    std::cout << object << " could not identified as file or directory\n";
            }
        }
    }
    if(cmdOptions.sort) {
        if(cmdOptions.verbose)
            std::cout << prog << ": sorting files and directories alphabetically...\n";
        std::sort(work_data.files.begin(), work_data.files.end());
        std::sort(work_data.dirs.begin(), work_data.dirs.end());
    }
}

void // get the contents of the given path
get_content() {
    struct dirent *ent;
    std::string of;
    if (cmdOptions.outfile_name .find("/") == std::string::npos) {
        of = cmdOptions.outfile_name;
    }
    else {
        of = cmdOptions.outfile_name.substr(cmdOptions.outfile_name.find_last_of("/")+1, cmdOptions.outfile_name.size());
    }
    std::cout << of;
    while ((ent = readdir(cmdOptions.path)) != NULL) {
        // eliminate current dir and go back characters
        if((strcmp(ent->d_name, ".") == 0) || (strcmp(ent->d_name, "..")) == 0) {
            continue;
        }
        else if (strcmp(ent->d_name, of.c_str()) == 0 && !cmdOptions.include_index) 
            continue;
        work_data.content.push_back(ent->d_name);
    }
}

void // the contents of the exclude file and add them to the exclude vector
get_exclude() {
    std::string l;
    // get the contents of the exclude file
    while (std::getline(cmdOptions.exfile, l)) {
        work_data.exclude.push_back(l);
    }
    // check if reading the file had any results
    // if not, give a warning that the file is empty
    if (work_data.exclude.empty()) {
        if(cmdOptions.verbose) 
            std::cout << prog << ": warning: the specified exclude file is empty!\n";
    }
}

int main(int argc, char* argv[]) {
    std::cout.sync_with_stdio(false);
    prog = argv[0];
    char buf[PATH_MAX];
    getcwd(buf, PATH_MAX);
    chdir(buf);
    cmdOptions.title = "Server";
    cmdOptions.charset = "utf-8";
    cmdOptions.headline_files = "Downloadable files";
    cmdOptions.headline_directories = "Browseable directories";
    cmdOptions.outfile_name = "index.html";
    cmdOptions.list_type = "ul";
    cmdOptions.include_index = false;
    cmdOptions.path_name = buf;
    cmdOptions.exfile_name = NULL;
    cmdOptions.sort = false;
    cmdOptions.verbose = false;

    int longIndex;
    int opt = getopt_long(argc, argv, optString, longOpts, &longIndex);
    while (opt != -1) {
        switch (opt) {
            case 't':
                cmdOptions.title = optarg;
                break;
            case 'c':
                cmdOptions.charset = optarg;
                break;
            case 'f':
                cmdOptions.headline_files = optarg;
                break;
            case 'd':
                cmdOptions.headline_directories = optarg;
                break;
            case 'p':
                cmdOptions.path_name = optarg;
                break;
            case 'o':
                cmdOptions.outfile_name = optarg;
                break;
            case 'l':
                if (!(strcmp(optarg, "ol") == 0) || !(strcmp(optarg, "ul") == 0)) {
                    std::cerr << prog << ": error: invalid argument: " << optarg << "\n" \
                    << prog << ": valid values: 'ul', 'ol'\n";
                    exit(EXIT_FAILURE);
                }
                cmdOptions.list_type = optarg;
                break;
            case 'i':
                cmdOptions.include_index = true;
                break;
            case 'e':
                cmdOptions.exfile_name = optarg;
                break;
            case 's':
                cmdOptions.sort = true;
            case 'v':
                cmdOptions.verbose = true;
                break;
            case 'h':
                help();
                break;
            case 'V':
                version();
                break;
            default:
                usage(EXIT_FAILURE);
                break;
        }
        opt = getopt_long(argc, argv, optString, longOpts, &longIndex);
    }
    // SETTING UP DEFAULT VALUES AND OPEN REQUIRED DIRECTORIES AND FILES
    // ###################################################

    if(cmdOptions.verbose)
    std::cout << prog << ": opening path...\n";

    // make sure you can open the the requested directory, otherwise exit
    cmdOptions.path = opendir(cmdOptions.path_name);
    if(cmdOptions.path == NULL) {
        std::cerr << prog << ": error: failed to open directory '" << \
        cmdOptions.path_name << "'\n";
        exit(EXIT_FAILURE);
    }

    // opening the output file and make sure the action succeded, otherwise exit
    cmdOptions.outfile.open(cmdOptions.outfile_name.c_str(), std::fstream::out | std::fstream::trunc);
    if (!cmdOptions.outfile.is_open()) {
        std::cerr << prog << ": error: couldn't open output file...\n";
        exit(EXIT_FAILURE);
    }
    
    // check for success in following functions
    if (cmdOptions.verbose)
        std::cout << prog << ": getting directory contents...\n";
    get_content();
    closedir(cmdOptions.path);
    
    // check if the user specified an exclude file
    // if so, open it and get it's contents
    if (cmdOptions.exfile_name != NULL) {
        cmdOptions.exfile.open(cmdOptions.exfile_name, std::fstream::in);
        if (!cmdOptions.exfile.is_open()) {
            std::cout << prog << ": error: failed to open exclude file '" << cmdOptions.exfile_name << "'\n";
            exit(EXIT_FAILURE);
        }
        else {
            // get the contents of the exclude file
            get_exclude();
            cmdOptions.exfile.close();
        }
    }
    //####################################################

    if (!work_data.exclude.empty()) {
        // exclude specified files from the content
        remove_exclude();
    }
    
    if (cmdOptions.verbose) 
        std::cout << prog << ": sorting content into directories and files...\n";
    sort_content();

    if (cmdOptions.verbose) 
        std::cout << prog << ": generating file content...\n";
    generate_file();

    if (cmdOptions.verbose)
        std::cout << prog << ": cleaning up...\n";
    cmdOptions.outfile.close();

    return 0;
}