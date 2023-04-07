#include <iostream>
#include "read_xml_arch_file.h"
#include "device_grid.h"
#include "pugixml.hpp"

enum{
    none = -1
};


struct parameters{
    int start_expr = -1;
    int repeat_expr = -1;
    int end_expr = -1;
    int incr_expr = -1;
};

struct element{

    int id = 0;
    std::string name = "";

    parameters x;
    parameters y;
    int w = -1;
    int h = -1;


//    element(const int id, const std::string name, const std::string x,
//            const std::string y, const std::string w, const std::string h)
//        : id(id), name(name), x(x), y(y), w(w), h(h) {}
};


void print_PhysicalTileTypes(const std::vector<t_physical_tile_type> &PhysicalTileTypes){
    std::cout << "**************************************" << std::endl;
    std::cout << "----------PhysicalTileTypes-----------" << std::endl;
    std::cout << "**************************************" << std::endl;
    for (const t_physical_tile_type &obj : PhysicalTileTypes){
        printf("name: %s\n", obj.name);
        printf("\t_width: %d\n", obj.width);
        printf("\t_height: %d\n", obj.height);
    }
}

void print_layout(const t_arch &arch){
    std::cout << "**************************************" << std::endl;
    std::cout << "----------------layout----------------" << std::endl;
    std::cout << "**************************************" << std::endl;
    for (const t_grid_def &obj : arch.grid_layouts){
        printf("name--------> %s\n", obj.name.c_str());
        printf("   width:        %d \n", obj.width);
        printf("   height:       %d \n", obj.height);
        printf("   aspect_ratio: %f (width/height)\n\n", obj.aspect_ratio);

        printf("   auto_layout\n");

        int i=0;
        for (const t_grid_loc_def &loc_def : obj.loc_defs){
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

void print_samples(const std::vector<element> &sample_elements){
    int count = 0;
    std::cout << "**************************************" << std::endl;
    std::cout << "-----------sample_elements------------" << std::endl;
    std::cout << "**************************************" << std::endl;
    for (const element &obj : sample_elements){
        printf("  %d\n", ++count);
        printf("  name: %s\n", obj.name.c_str());

        try{
            printf("\t_startx: %d\n", obj.x.start_expr);
        }catch(const char *e){}
        try{
            printf("\t_starty: %d\n\n", obj.y.start_expr);
        }catch(const char *e){}
        try{
            printf("\t_endx: %d\n", obj.x.end_expr);
        }catch(const char *e){}
        try{
            printf("\t_endy: %d\n\n", obj.y.end_expr);
        }catch(const char *e){}

        try{
            printf("\t_repeatx: %d\n", obj.x.repeat_expr);
        }catch(const char *e){}
        try{
            printf("\t_repeaty: %d\n\n", obj.y.repeat_expr);
        }catch(const char *e){}

    }
}

void print_file_xml(const std::vector<element> &sample_elements){
    pugi::xml_document doc;
    pugi::xml_node elements = doc.append_child("elements");

    for (const element &obj : sample_elements){
        pugi::xml_node element = elements.append_child("element");

        element.append_attribute("id") = obj.id;

        element.append_attribute("x") = obj.x.start_expr;
        element.append_attribute("y") = obj.y.start_expr;

        element.append_attribute("height") = obj.h;
        element.append_attribute("width") = obj.w;
    }

    doc.save_file("output.xml");
}

bool try_catch(std::string &sample_el){
    bool result = false;
    if (std::all_of(sample_el.begin(), sample_el.end(), :: isdigit)) {
        result = true;
    }
    return result;
}

void arch_to_vector(t_arch &arch, std::vector<element> &sample_elements){

    for (t_grid_def &layout : arch.grid_layouts){
        for (t_grid_loc_def &sample_el : layout.loc_defs){

            element temp;

            temp.name = sample_el.block_type;

            if (try_catch(sample_el.x.start_expr))
                temp.x.start_expr = stoi(sample_el.x.start_expr);
            if (try_catch(sample_el.x.end_expr))
                temp.x.end_expr = stoi(sample_el.x.end_expr);
            if (try_catch(sample_el.x.incr_expr))
                temp.x.incr_expr = stoi(sample_el.x.incr_expr);
            if (try_catch(sample_el.x.repeat_expr))
                temp.x.repeat_expr = stoi(sample_el.x.repeat_expr);

            if (try_catch(sample_el.y.start_expr))
                temp.y.start_expr = stoi(sample_el.y.start_expr);
            if (try_catch(sample_el.y.end_expr))
                temp.y.end_expr = stoi(sample_el.y.end_expr);
            if (try_catch(sample_el.y.incr_expr))
                temp.y.incr_expr = stoi(sample_el.y.incr_expr);
            if (try_catch(sample_el.y.repeat_expr))
                temp.y.repeat_expr = stoi(sample_el.y.repeat_expr);

            sample_elements.push_back(temp);
        }
    }


}

void adding_wh(std::vector<t_physical_tile_type> &PhysicalTileTypes, std::vector<element> &sample_elements){
    for (element &element : sample_elements){
        for (t_physical_tile_type &physical_element : PhysicalTileTypes){
            if (element.name == physical_element.name){
                element.w = physical_element.width;
                element.h = physical_element.height;
            }
        }
    }
}

void claster(std::vector<element> &sample_elements){
    int i = 0;
    std::vector<element> temp;

    for (element &obj : sample_elements){
        int index_of_element;
        if (obj.name != "EMPTY" && obj.name != "io" && obj.name != "clb") {
            if (obj.x.end_expr != none) {
                index_of_element = i;
                // the number of elements which consists in x,y axes
                int length_x = (obj.x.end_expr - obj.x.start_expr) / obj.w;
                int length_y = (obj.y.end_expr - obj.y.start_expr) / obj.h;

                // the amount of elements in claster
                int count = length_x * length_y;

                int x = 0, y = 0;
                for (int num = 0; num < count; ++num) {
                    element sub_element;

                    // adding constant parameters
                    sub_element = obj;
                    sub_element.name = obj.name;
                    //                sub_element.w = obj.w;
                    //                sub_element.h = obj.h;
                    //                sub_element.x.repeat_expr = obj.x.repeat_expr;
                    //                sub_element.y.repeat_expr = obj.y.repeat_expr;

                    // adding variable parameters
                    sub_element.x.start_expr = obj.x.start_expr + x * obj.w;
                    sub_element.y.start_expr = obj.y.start_expr + y * obj.h;

                    x += 1;
                    if (x * obj.w == length_x) {
                        x = 0;
                        y += 1;
                    }

                    temp.push_back(sub_element);
                }
                sample_elements.erase(sample_elements.begin() + index_of_element);
            }
            i += 1;
        }
    }

    for (element &obj : temp){
        sample_elements.push_back(obj);
    }

}

void repeating(t_arch &arch, std::vector<element> &sample_elements){

    std::vector<std::string> name_if_x;
    std::vector<std::string> name_if_y;

    // adding the names of duplicate elements
    for (element &obj : sample_elements){
        if (obj.name != "EMPTY" && obj.name != "io" && obj.name != "clb") {
            if (obj.x.repeat_expr != none) {
                name_if_x.push_back(obj.name);
            }
            if (obj.y.repeat_expr != none) {
                name_if_y.push_back(obj.name);
            }
        }
    }

    if (!name_if_x.empty()){
        int index_in_sample_el;
        element temp;

        // finding the name of the element
        for (std::string &name : name_if_x) {
            int index = 0;
            std::vector<element> created_elements;
            index_in_sample_el = 0;
            for (element& obj : sample_elements) {
                if (obj.name == name) {
                    temp = obj;
                    index_in_sample_el = index;
                }
                index += 1;
            }

            // the amount of repeating clasters
            int count = (arch.grid_layouts[0].width - temp.x.start_expr) / temp.x.repeat_expr;

            //assigning the appropriate coordinates
            for (int i = 0; i < count; ++i) {
                element temp_created_elements;

                temp_created_elements = temp;
                temp_created_elements.name = temp.name;

                temp_created_elements.x.start_expr = temp_created_elements.x.start_expr + i * temp.x.repeat_expr;
                temp_created_elements.x.end_expr = temp_created_elements.x.end_expr + i * temp.x.repeat_expr;

                created_elements.push_back(temp_created_elements);
            }
            sample_elements.erase(sample_elements.begin()+index_in_sample_el);
            for (element &obj : created_elements){
                sample_elements.push_back(obj);
            }
        }

    }

    if (!name_if_y.empty()){
        int index_in_sample_el;
        element temp;


        // finding the name of the element
        for (std::string &name : name_if_y) {
            int index = 0;
            std::vector<element> created_elements;
            index_in_sample_el = 0;
            for (element& obj : sample_elements) {
                if (obj.name == name) {
                    temp = obj;
                    index_in_sample_el = index;
                }
                index += 1;
            }

            // the amount of repeating clasters
            int count = (arch.grid_layouts[0].height - temp.y.start_expr) / temp.y.repeat_expr;

            //assigning the appropriate coordinates
            for (int i = 0; i < count; ++i) {
                element temp_created_elements;

                temp_created_elements = temp;
                temp_created_elements.name = temp.name;

                temp_created_elements.y.start_expr = temp_created_elements.y.start_expr + i * temp.y.repeat_expr;
                temp_created_elements.y.end_expr = temp_created_elements.y.end_expr + i * temp.y.repeat_expr;

                created_elements.push_back(temp_created_elements);
            }

            sample_elements.erase(sample_elements.begin()+index_in_sample_el);
            for (element &obj : created_elements){
                sample_elements.push_back(obj);
            }
        }
    }
}

void set_grid(t_arch &arch, std::vector<std::vector<element>> &grid){
    element none;
    int WIDTH = arch.grid_layouts[0].width;
    int HEIGHT = arch.grid_layouts[0].height;

    // grid initialization
    for (int i = 0; i < HEIGHT; ++i){
        std::vector<element> temp;
        for (int j = 0; j < WIDTH; ++j){
            temp.push_back(none);
        }
        grid.push_back(temp);
    }
}


void print_grid(std::vector<std::vector<element>> &grid){
    for (std::vector<element> &raw : grid){
        for(element& obj : raw){
            std::cout << obj.name << "| ";
        }
        std::cout << std::endl;
    }
}


int main(){
    const char* arch_filename = "fixed_layout.xml";

    // Load architecture from XML file
    t_arch arch;
    bool timing_enabled = true;
    std::vector<t_physical_tile_type> PhysicalTileTypes;
    std::vector<t_logical_block_type> LogicalBlockTypes;

    XmlReadArch(arch_filename, timing_enabled, &arch, PhysicalTileTypes, LogicalBlockTypes);
    //info
    print_PhysicalTileTypes(PhysicalTileTypes);
    print_layout(arch);

    std::vector<element> sample_elements;
    std::vector<std::vector<element>> grid;

    arch_to_vector(arch, sample_elements);
    adding_wh(PhysicalTileTypes, sample_elements);

    repeating(arch, sample_elements);
    claster(sample_elements);

    print_samples(sample_elements);
    print_file_xml(sample_elements);

    set_grid(arch, grid);

    

    print_grid(grid);

    return 0;
}
