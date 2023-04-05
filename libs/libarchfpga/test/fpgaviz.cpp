#include <iostream>
#include "read_xml_arch_file.h"
#include "device_grid.h"

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

void print_samples(const std::vector<element> &sample_elements){
    std::cout << "**************************************" << std::endl;
    std::cout << "-----------sample_elements------------" << std::endl;
    std::cout << "**************************************" << std::endl;
    for (const element &obj : sample_elements){
        printf("name: %s\n", obj.name.c_str());

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
            printf("\t_endy: %d\n\n", obj.y.end_expr);;
        }catch(const char *e){}

        try{
            printf("\t_repeatx: %d\n", obj.x.repeat_expr);
        }catch(const char *e){}
        try{
            printf("\t_repeaty: %d\n\n", obj.y.repeat_expr);
        }catch(const char *e){}

    }
}

int main() {
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

    print_samples(sample_elements);


    return 0;
}
