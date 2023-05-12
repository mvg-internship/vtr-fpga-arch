#include <cctype>
#include "device_grid.h"
#include <iostream>
#include "pugixml.hpp"
#include "read_xml_arch_file.h"


enum {
    none = -1
};

struct parameters {
    float start_expr = -1;
    float repeat_expr = -1;
    float end_expr = -1;
    float incr_expr = -1;
  
    parameters() = default;

    parameters(const parameters& other)
        : start_expr(other.start_expr),
        repeat_expr(other.repeat_expr),
        end_expr(other.end_expr),
        incr_expr(other.incr_expr) {}
};

struct element {

    int id = 0;
    std::string name = "";

    parameters x;
    parameters y;
    float w = -1;
    float h = -1;
    int priority = -1;

    element() = default;

    element(const element& other)
        : id(other.id), name(other.name), x(other.x), y(other.y),
        w(other.w), h(other.h), priority(other.priority) {}
};

// functions for testing and debugging
void print_PhysicalTileTypes(const std::vector<t_physical_tile_type> &PhysicalTileTypes) {
    printf("**************************************\n");
    printf("----------PhysicalTileTypes-----------\n");
    printf("**************************************\n");
    for (const t_physical_tile_type &obj : PhysicalTileTypes) {
        printf("name: %s\n", obj.name);
        printf("\t_width: %d\n", obj.width);
        printf("\t_height: %d\n", obj.height);
    }
}

void print_layout(const t_arch &arch) {
    printf("**************************************\n");
    printf("----------------layout----------------\n");
    printf("**************************************\n");
    for (const t_grid_def &obj : arch.grid_layouts) {
        printf("name--------> %s\n", obj.name.c_str());
        printf("   width:        %d \n", obj.width);
        printf("   height:       %d \n", obj.height);
        printf("   aspect_ratio: %f (width/height)\n\n", obj.aspect_ratio);

        printf("   auto_layout\n");

        int i = 0;
        for (const t_grid_loc_def &loc_def : obj.loc_defs) {
            printf("   %d\n", ++i);
            printf("\t_type: %s\n", loc_def.block_type.c_str());

            printf("\t_startx: %s\n", loc_def.x.start_expr.c_str());
            printf("\t_starty: %s\n\n", loc_def.y.start_expr.c_str());
            printf("\t_endx: %s\n", loc_def.x.end_expr.c_str());
            printf("\t_endy: %s\n\n", loc_def.y.end_expr.c_str());

            printf("\t_repeatx: %s\n", loc_def.x.repeat_expr.c_str());
            printf("\t_repeaty: %s\n\n", loc_def.y.repeat_expr.c_str());

            printf("\t_priority: %d\n", loc_def.priority);
        }
    }
}

void print_samples(const std::vector<element> &sample_elements) {
    int count = 0;

    printf("**************************************\n");
    printf("-----------sample_elements------------\n");
    printf("**************************************\n");

    for (const element &obj : sample_elements) {
        printf("  %d\n", ++count);
        printf("  name: %s\n", obj.name.c_str());

        try {
            printf("\t_startx: %.1f\n", obj.x.start_expr);
        } catch(const char *e) {}
        try {
            printf("\t_starty: %.1f\n\n", obj.y.start_expr);
        } catch(const char *e) {}
        try {
            printf("\t_endx: %.1f\n", obj.x.end_expr);
        } catch(const char *e) {}
        try {
            printf("\t_endy: %.1f\n\n", obj.y.end_expr);
        } catch(const char *e) {}

        try {
            printf("\t_repeatx: %.1f\n", obj.x.repeat_expr);
        } catch(const char *e) {}
        try {
            printf("\t_repeaty: %.1f\n\n", obj.y.repeat_expr);
        } catch(const char *e) {}
        printf("\t_w: %.1f\n", obj.w);
        printf("\t_h: %.1f\n", obj.h);
        printf("\t_priority: %d\n\n", obj.priority);
    }
}

void print_element(const element &temp) {
    printf("  name: %s\n", temp.name.c_str());

    try {
        printf("\t_id: %d\n", temp.id);
    } catch(const char *e) {}

    printf("\t_startx: %.1f\n", temp.x.start_expr);
    printf("\t_starty: %.1f\n\n", temp.y.start_expr);

    printf("\t_w: %.1f\n", temp.w);
    printf("\t_h: %.1f\n", temp.h);
    try {
        printf("\t_priority: %d\n\n", temp.priority);
    } catch(const char *e) {}
}

void print_new_elements(const std::vector<element> &new_elements) {
    printf("**************************************\n");
    printf("-------------new_elements-------------\n");
    printf("**************************************\n");
    for (const element &obj : new_elements) {
        print_element(obj);
    }
}

void print_grid(const std::vector<std::vector<element>> &grid) {
    for (const std::vector<element> &raw : grid) {
        for(const element &obj : raw) {
            printf("%f %f|", obj.y.start_expr, obj.x.start_expr);
        }
        printf("\n");
    }
}

void print_names_grid(const std::vector<std::vector<element>> &grid) {
    printf("****************************************************\n");
    printf("--------------------names_grid----------------------\n");
    printf("****************************************************\n");

    for (const std::vector<element> &raw : grid) {
        for(const element &obj : raw) {
            printf(" %s |", obj.name.c_str());
        }
        printf("\n");
    }
}

void print_coordinates_grid(const std::vector<std::vector<element>> &grid) {
    for (const std::vector<element> &raw : grid) {
        for(const element &obj : raw) {
            printf("%.1f %.1f |", obj.y.start_expr, obj.x.start_expr);
        }
        printf("\n");
    }
}


//// required functions for data processing
void parse_file_xml(const char *path, t_arch &arch, std::vector<t_physical_tile_type> &PhysicalTileTypes) {
    const char *arch_filename = path;
    bool timing_enabled = true;
    std::vector<t_logical_block_type> LogicalBlockTypes;
    // parsing XML-file
    XmlReadArch(arch_filename, timing_enabled, &arch, PhysicalTileTypes, LogicalBlockTypes);
}

void print_file_xml(const char *path_save, const std::vector<element> &normalized_elements) {
    pugi::xml_document doc;
    pugi::xml_node logic_scheme = doc.append_child("logic_scheme");
    pugi::xml_node elements = logic_scheme.append_child("elements");

    for (const element &obj : normalized_elements) {
        pugi::xml_node element = elements.append_child("element");

        element.append_attribute("e_id") = obj.id;

        element.append_attribute("x") = obj.x.start_expr;
        element.append_attribute("y") = obj.y.start_expr;

        element.append_attribute("height") = obj.h;
        element.append_attribute("width") = obj.w;
    }
    doc.save_file(path_save);
}

bool try_catch(const std::string &sample_el) {
    return std::all_of(sample_el.begin(), sample_el.end(), :: isdigit);
}

void arch_to_vector(const t_arch &arch, std::vector<element> &sample_elements) {
    int WIDTH = arch.grid_layouts[0].width;
    int HEIGHT = arch.grid_layouts[0].height;

    for (const t_grid_def &layout : arch.grid_layouts) {
        for (const t_grid_loc_def &sample_el : layout.loc_defs) {
            element temp;

            temp.name = sample_el.block_type;

            if (try_catch(sample_el.x.start_expr))
                temp.x.start_expr = std::stof(sample_el.x.start_expr);
            if (try_catch(sample_el.x.end_expr))
                temp.x.end_expr = std::stof(sample_el.x.end_expr);
            if (try_catch(sample_el.x.incr_expr))
                temp.x.incr_expr = std::stof(sample_el.x.incr_expr);
            if (try_catch(sample_el.x.repeat_expr))
                temp.x.repeat_expr = std::stof(sample_el.x.repeat_expr);

            if (try_catch(sample_el.y.start_expr))
                temp.y.start_expr = std::stof(sample_el.y.start_expr);
            if (try_catch(sample_el.y.end_expr))
                temp.y.end_expr = std::stof(sample_el.y.end_expr);
            if (try_catch(sample_el.y.incr_expr))
                temp.y.incr_expr = std::stof(sample_el.y.incr_expr);
            if (try_catch(sample_el.y.repeat_expr))
                temp.y.repeat_expr = std::stof(sample_el.y.repeat_expr);

            temp.priority = sample_el.priority;

            sample_elements.push_back(temp);
        }
    }

    for (element &obj : sample_elements) {
        if (obj.name == "EMPTY" && obj.priority == 101) {
            if (obj.x.start_expr == -1)
                obj.x.start_expr = (float) WIDTH;
            if (obj.y.start_expr == -1)
                obj.y.start_expr = (float) HEIGHT;
        }
    }
}

void add_wh(const std::vector<t_physical_tile_type> &PhysicalTileTypes, std::vector<element> &sample_elements) {
    for (element &element : sample_elements) {
        for (const t_physical_tile_type &physical_element : PhysicalTileTypes) {
            if (element.name == physical_element.name) {
                element.w = (float) physical_element.width;
                element.h = (float) physical_element.height;
            }
        }
    }
}

void repeate(const t_arch &arch, std::vector<element> &sample_elements) {
    int WIDTH = arch.grid_layouts[0].width;
    int HEIGHT = arch.grid_layouts[0].height;

    std::vector<element> temp;

    // adding the names of duplicated elements
    for (element &obj : sample_elements) {
        if (obj.name != "io" && obj.name != "clb") {
            temp.push_back(obj);
        }
    }

    if (!temp.empty()) {

        // the process of creating duplicate elements as independent elements
        for (element &obj : temp) {
            std::vector<element> local_created_elements;

            // if the element repeats relative to two axes at the same time
            if ((obj.x.repeat_expr != -1.0 && obj.x.repeat_expr != 0) &&
                (obj.y.repeat_expr != -1.0 && obj.y.repeat_expr != 0)) {
                int start_x = (int) obj.x.start_expr - 1;
                int repeat_x = (int) obj.x.repeat_expr;
                int end_x = (int) obj.x.end_expr;

                auto difference_x = (float) (end_x - start_x);

                int count_x = (WIDTH - start_x) / repeat_x;
                int second_x = start_x + repeat_x;
                int end_point_x = start_x + repeat_x * count_x;
                if ((end_point_x + (int) difference_x) >= (WIDTH - 1))
                    end_point_x = start_x + repeat_x * (count_x - 1);

                int start_y = (int) obj.y.start_expr - 1;
                int repeat_y = (int) obj.y.repeat_expr;
                int end_y = (int) obj.y.end_expr;

                auto difference_y = (float) (end_y - start_y);

                int count_y = (HEIGHT - start_y) / repeat_y;
                int second_y = start_y + repeat_y;
                int end_point_y = start_y + repeat_y * count_y;
                if ((end_point_y + (int) difference_y) >= (HEIGHT - 1))
                    end_point_y = start_y + repeat_y * (count_y - 1);


                for (int i = second_y; i <= end_point_y; i += repeat_y) {
                    element temp_element_y_axis = obj;

                    temp_element_y_axis.y.start_expr = (float) i;
                    if (temp_element_y_axis.y.end_expr != 0 && temp_element_y_axis.x.end_expr != -1.0)
                        temp_element_y_axis.y.end_expr = (float) i + difference_x;

                    for (int j = second_x; j <= end_point_x; j += repeat_x) {
                        element temp_element_x_axis = temp_element_y_axis;
                        temp_element_x_axis.x.start_expr = (float) j;
                        if (temp_element_x_axis.x.end_expr != 0 && temp_element_x_axis.x.end_expr != -1.0)
                            temp_element_x_axis.x.end_expr = (float) j + difference_x;
                        local_created_elements.push_back(temp_element_x_axis);
                    }
                }
            }

            //if the element repeats relative to the x axis
            else if (obj.x.repeat_expr != -1.0 && obj.x.repeat_expr != 0) {
                int start_x = (int) obj.x.start_expr - 1;
                int repeat_x = (int) obj.x.repeat_expr;
                int end_x = (int) obj.x.end_expr;

                auto difference_x = (float) (end_x - start_x);

                int count_x = (WIDTH - start_x) / repeat_x;
                int second_point = start_x + repeat_x;
                int end_point = start_x + repeat_x * count_x;
                if ((end_point + (int) difference_x) >= (WIDTH - 1))
                    end_point = start_x + repeat_x * (count_x - 1);


                for (int i = second_point; i <= end_point; i += repeat_x) {
                    element temp_element_x_axis = obj;
                    temp_element_x_axis.x.start_expr = (float) i;
                    if (temp_element_x_axis.x.end_expr != 0 && temp_element_x_axis.x.end_expr != -1.0)
                        temp_element_x_axis.x.end_expr = (float) i + difference_x;
                    local_created_elements.push_back(temp_element_x_axis);
                }

            }

            //if the element repeats relative to the y axis
            else if (obj.y.repeat_expr != -1.0 && obj.y.repeat_expr != 0) {
                int start_y = (int) obj.y.start_expr - 1;
                int repeat_y = (int) obj.y.repeat_expr;
                int end_y = (int) obj.y.end_expr;

                auto difference_y = (float) (end_y - start_y);

                int count_y = (HEIGHT - start_y) / repeat_y;
                int second_y = start_y + repeat_y;
                int end_point = start_y + repeat_y * count_y;
                if ((end_point + (int) difference_y) >= (HEIGHT - 1))
                    end_point = start_y + repeat_y * (count_y - 1);


                for (int i = second_y; i <= end_point; i += repeat_y) {
                    element temp_element_y_axis = obj;
                    temp_element_y_axis.y.start_expr = (float) i;
                    if (temp_element_y_axis.x.end_expr != 0 && temp_element_y_axis.x.end_expr != -1.0)
                        temp_element_y_axis.x.end_expr = (float) i + difference_y;
                    local_created_elements.push_back(temp_element_y_axis);
                }
            }

            // adding created elements to the main array of elements
            if(!local_created_elements.empty()) {
                for (element &local_created_element : local_created_elements)
                    sample_elements.push_back(local_created_element);
            }
        }
    }
}

void cluster(std::vector<element> &sample_elements) {
    std::vector<element> divided_elements;

    for (element &sample_element : sample_elements) {
        auto index = sample_elements.begin();
        // condition for processing elements of all types except io, club and EMPTY that have priority below 101
        if (sample_element.name != "io" && sample_element.name != "clb" && sample_element.priority!= 101) {

            // if the element occupies an area of more than one on 2 axes at the same time
            if ((sample_element.x.end_expr != none && sample_element.y.end_expr != none) ||
                (sample_element.x.end_expr != 0 && sample_element.y.end_expr != 0)) {

                // the number of elements which consists in x,y axes
                float length_x = (sample_element.x.end_expr - sample_element.x.start_expr - 1) / sample_element.w;
                float length_y = (sample_element.y.end_expr - sample_element.y.start_expr - 1) / sample_element.h;

                //the division of this cluster by the number of elements
                // (repetitions of width in x axis / height in y axis) contained on each axis
                for (int i = 0; i < (int) length_y; i += (int) sample_element.h) {
                    for (int j = 0; j < (int) length_x; j += (int) sample_element.w) {
                        element created_element(sample_element);

                        created_element.x.start_expr = sample_element.x.start_expr + (float) j;
                        created_element.y.start_expr = sample_element.y.start_expr + (float) i;

                        divided_elements.push_back(created_element);
                    }
                }
            }

            // if the element occupies an area of more than one on x axis
            else if (sample_element.x.end_expr != none || sample_element.x.end_expr != 0 ) {

                // the amount of elements in cluster
                float num_elements = (sample_element.x.end_expr - sample_element.x.start_expr - 1) / sample_element.w;

                //the division of this cluster by the number of elements
                // (repetitions of width in x axis) contained on each axis
                for (int i = 0; i < (int) num_elements; i += (int) sample_element.w) {
                    element created_element(sample_element);

                    created_element.x.start_expr = sample_element.x.start_expr + (float) i;

                    divided_elements.push_back(created_element);
                }
            }

            // if the element occupies an area of more than one on y axis
            else if (sample_element.y.end_expr != none || sample_element.y.end_expr != 0 ) {

                // the amount of elements in cluster
                float num_elements = (sample_element.y.end_expr - sample_element.y.start_expr - 1) / sample_element.h;

                //the division of this cluster by the number of elements
                // (repetitions of height in y axis) contained on each axis
                for (int i = 0; i < (int) num_elements; i += (int) sample_element.h) {
                    element created_element(sample_element);

                    created_element.y.start_expr = sample_element.y.start_expr + (float) i;

                    divided_elements.push_back(created_element);
                }
            }

        }
    }

    // removing duplicate elements from the used array
    for (element &divided_element : divided_elements) {
        auto index = sample_elements.begin();
        for (element &sample_element : sample_elements) {
            if (sample_element.x.start_expr == divided_element.x.start_expr &&
                sample_element.y.start_expr == divided_element.y.start_expr &&
                sample_element.x.end_expr == divided_element.x.end_expr &&
                sample_element.y.end_expr == divided_element.y.end_expr) {
                sample_elements.erase(index);
            }
            ++index;
        }
    }

    // adding created independent elements
    for (element &divided_element : divided_elements) {
        sample_elements.push_back(divided_element);
    }
}

void fill_clb(const t_arch &arch, std::vector<element> &sample_elements, std::vector<std::vector<element>> &grid) {
    int WIDTH = arch.grid_layouts[0].width;
    int HEIGHT = arch.grid_layouts[0].height;

    element clb;

    // search for a clb element that is always stored in a single instance
    for (element& obj : sample_elements) {
        if(obj.name == "clb") {
            clb = obj;
        }
    }

    // filling in the grid by clb element
    for (int i = 0; i < HEIGHT; ++i) {
        std::vector<element> temp;
        for (int j = 0; j < WIDTH; ++j) {
            temp.push_back(clb);
        }
        grid.push_back(temp);
    }
}

void fill_io(const t_arch &arch, std::vector<element> &sample_elements, std::vector<std::vector<element>> &grid) {
    int WIDTH = arch.grid_layouts[0].width;
    int HEIGHT = arch.grid_layouts[0].height;

    element io;

    // search for io element to fill the grid around the perimeter
    for (element &obj : sample_elements) {
        if(obj.name == "io") {
            io = obj;
        }
    }

    // filling the grid around the perimeter by io elements
    for (size_t i = 0; i < (size_t) HEIGHT ; ++i) {
        for (size_t j = 0; j < (size_t) WIDTH ; ++j) {
            if (i == 0 || (int) i == (HEIGHT - 1) || j == 0 || (int) j == (WIDTH - 1))
                if (grid[i][j].priority < io.priority)
                    grid[i][j] = io;
        }
    }
}

void fill_empty(const t_arch &arch, std::vector<element> &sample_elements, std::vector<std::vector<element>> &grid) {
    int WIDTH = arch.grid_layouts[0].width;
    int HEIGHT = arch.grid_layouts[0].height;

    std::vector<element> empty_elements;

    // search for all EMPTY elements
    for (element &obj : sample_elements) {
        if(obj.name == "EMPTY"){
            empty_elements.push_back(obj);
        }
    }

    // filling the grid with EMPTY elements
    for (size_t i = 0; i < (size_t) HEIGHT ; ++i) {
        for (size_t j = 0; j < (size_t) WIDTH ; ++j) {
            for (element &obj : empty_elements){
                if (grid[i][j].priority < obj.priority) {
                    if (obj.x.start_expr == (float) 0 && obj.y.start_expr == (float) 0)

                        grid[HEIGHT - 1][0] = obj;
                    else if (obj.x.start_expr == (float) WIDTH && obj.y.start_expr == 0)
                        grid[HEIGHT - 1][WIDTH - 1] = obj;
                    else if (obj.x.start_expr == 0 && obj.y.start_expr == (float) HEIGHT)
                        grid[0][0] = obj;
                    else if (obj.x.start_expr == (float) WIDTH && obj.y.start_expr == (float) HEIGHT)
                        grid[0][WIDTH - 1] = obj;
                }
            }
        }
    }

}

void fill_others(const t_arch &arch, std::vector<element> &sample_elements, std::vector<std::vector<element>> &grid) {
    int WIDTH = arch.grid_layouts[0].width;
    int HEIGHT = arch.grid_layouts[0].height;

    element none;
    std::vector<element> other_elements;

    // search for elements that are located depending on the FPGA architecture
    for (element &obj : sample_elements) {
        if(obj.name != "io" && obj.name != "clb" && obj.name != "EMPTY") {
            other_elements.push_back(obj);
        }
    }
    // filling the grid with these elements
    for (size_t i = 0; i < (size_t) HEIGHT ; ++i) {
        for (size_t j = 0; j < (size_t) WIDTH ; ++j) {
            for (element &obj : other_elements) {

                // checking for a conflict of priorities of elements that are located on the same tile
                // "child" - is the area occupied by an element that has dimensions other than one
                // "child" is needed to find the first occurrence of each independent element
                if (grid[i][j].priority < obj.priority) {
                    if ((float) j == obj.x.start_expr && i == (size_t) (HEIGHT - (int) obj.y.start_expr - 1)) {
                        grid[i][j] = obj;

                        // checking the area to be filled by an element by its height
                        if (obj.h != 1) {
                            grid[i][j].name = "child";

                            for (size_t h = 0; h < (size_t) obj.h - 1; ++h) {
                                grid[i - h - 1][j] = obj;
                                if(h != (size_t) (obj.h - 2))
                                    grid[i - h - 1][j].name = "child";
                            }
                        }
                        // checking the area to be filled by an element by its width
                        else if (obj.w != 1) {
                            grid[i][j].name = "child";

                            for (size_t w = 0; w < (size_t) obj.w-1; ++w) {
                                grid[i - 1][j + w + 1] = obj;
                                if(w != (size_t) (obj.w - 2))
                                    grid[i - 1][j + w + 1].name = "child";
                            }
                        }
                        // checking the area to be filled by an element
                        // by its height and width at the same time
                        else if (obj.h != 1 && obj.w != 1) {
                            grid[i][j].name = "child";

                            for (size_t h = 0; h < (size_t) obj.h - 1; ++h) {
                                for (size_t w = 0; w < (size_t) obj.w - 1; ++w) {
                                    grid[i - h - 1][j + w + 1] = obj;
                                    if(h != (size_t) (obj.h - 2) && w != (size_t) (obj.w - 2))
                                        grid[i - h - 1][j + w + 1].name = "child";
                                }
                            }
                        }

                    }
                }

            }
        }
    }

}

void assign_coordinates (const t_arch &arch, std::vector<element> &normalized_elements, std::vector<std::vector<element>> &grid) {
    int WIDTH = arch.grid_layouts[0].width;
    int HEIGHT = arch.grid_layouts[0].height;
    float incr = 0.2; // distance between adjacent elements (used for correct display)

    // assignment of new coordinates in accordance with the already received grid
    for (size_t i = 0; i < (size_t) HEIGHT ; ++i) {
        for (size_t j = 0; j < (size_t) WIDTH ; ++j) {
            element temp;
            // "child" and EMPTY elements are not added to the list because they should not be drawn
            if (grid[i][j].name != "child" && grid[i][j].name != "EMPTY") {
                temp = grid[i][j];
                // normalization of coordinates in accordance with the requirements:
                // x,y,w,h = [0; 1]
                temp.x.start_expr =  (float) j / static_cast<float> (WIDTH);
                temp.y.start_expr = (float) i / static_cast<float> (HEIGHT);
                temp.w = (temp.w - incr) / static_cast<float> (WIDTH);
                temp.h = (temp.h - incr) / static_cast<float> (HEIGHT);
                normalized_elements.push_back(temp);
            }
        }
    }
}

void set_id(std::vector<element> &normalized_elements) {
    int id=0;
    for (element &obj : normalized_elements) {
        obj.id = id;
        id += 1;
    }
}

int main(int argc, char *argv[]) {
    t_arch arch;
    std::vector<t_physical_tile_type> PhysicalTileTypes;
    std::vector<element> sample_elements;
    std::vector<element> normalized_elements;
    std::vector<std::vector<element>> grid;

    // Load architecture from XML file
    parse_file_xml(argv[1], arch, PhysicalTileTypes);

    // info for testing and debugging
    print_PhysicalTileTypes(PhysicalTileTypes);
    print_layout(arch);

    // processing of the received data
    arch_to_vector(arch, sample_elements);
    add_wh(PhysicalTileTypes, sample_elements);
    repeate(arch, sample_elements);
    cluster(sample_elements);

    // filling the grid with processed data
    fill_clb(arch, sample_elements, grid);
    fill_io(arch, sample_elements, grid);
    print_samples(sample_elements);
    fill_empty(arch, sample_elements, grid);
    fill_others(arch, sample_elements, grid);

    // assigning new coordinates and identifiers
    assign_coordinates(arch, normalized_elements, grid);
    set_id(normalized_elements);

    // info for testing and debugging (the function of
    // displaying a grid filled with the names of elements)
    print_names_grid(grid);

    // printing of processed FPGA architecture info
    // in xml-format in accordance with the requirements
    print_file_xml(argv[2], normalized_elements);

    return 0;
}
