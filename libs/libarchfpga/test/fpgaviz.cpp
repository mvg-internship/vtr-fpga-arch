#include <iostream>
#include <cctype>
#include "read_xml_arch_file.h"
#include "device_grid.h"
#include "pugixml.hpp"

enum{
    none = -1
};


struct parameters{
    float start_expr = -1;
    float repeat_expr = -1;
    float end_expr = -1;
    float incr_expr = -1;
};

struct element{

    int id = 0;
    std::string name = "";

    parameters x;
    parameters y;
    float w = -1;
    float h = -1;


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
            printf("\t_startx: %.1f\n", obj.x.start_expr);
        }catch(const char *e){}
        try{
            printf("\t_starty: %.1f\n\n", obj.y.start_expr);
        }catch(const char *e){}
        try{
            printf("\t_endx: %.1f\n", obj.x.end_expr);
        }catch(const char *e){}
        try{
            printf("\t_endy: %.1f\n\n", obj.y.end_expr);
        }catch(const char *e){}

        try{
            printf("\t_repeatx: %.1f\n", obj.x.repeat_expr);
        }catch(const char *e){}
        try{
            printf("\t_repeaty: %.1f\n\n", obj.y.repeat_expr);
        }catch(const char *e){}
        printf("\t_w: %.1f\n", obj.w);
        printf("\t_h: %.1f\n\n", obj.h);

    }
}

void print_file_xml(const std::vector<element> &new_elements){

    pugi::xml_document doc;
    pugi::xml_node logic_scheme = doc.append_child("logic_scheme");
    pugi::xml_node elements = logic_scheme.append_child("elements");


    for (const element &obj : new_elements){
        pugi::xml_node element = elements.append_child("element");

        element.append_attribute("e_id") = obj.id;

        element.append_attribute("x") = obj.x.start_expr;
        element.append_attribute("y") = obj.y.start_expr;

        element.append_attribute("height") = obj.h;
        element.append_attribute("width") = obj.w;
    }

    doc.save_file("/Users/fake.andrey/basicviz/basicviz/build/src/output.xml");
}

bool try_catch(const std::string &sample_el){
    return std::all_of(sample_el.begin(), sample_el.end(), :: isdigit);
}

void arch_to_vector(const t_arch &arch, std::vector<element> &sample_elements){
    int WIDTH = arch.grid_layouts[0].width;
    int HEIGHT = arch.grid_layouts[0].height;

    for (const t_grid_def &layout : arch.grid_layouts){
        for (const t_grid_loc_def &sample_el : layout.loc_defs){
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


            sample_elements.push_back(temp);
        }
    }

    for (element &obj : sample_elements){
        if (obj.name == "EMPTY"){
            if (obj.x.start_expr == -1)
                obj.x.start_expr = (float)WIDTH;
            if (obj.y.start_expr == -1)
                obj.y.start_expr = (float)HEIGHT;
            if (obj.x.end_expr == -1)
                obj.x.end_expr = none;
            if (obj.y.end_expr == -1)
                obj.y.end_expr = -1;
        }

    }


}

void adding_wh(std::vector<t_physical_tile_type> &PhysicalTileTypes, std::vector<element> &sample_elements){

    for (element &element : sample_elements){
        for (t_physical_tile_type &physical_element : PhysicalTileTypes){
            if (element.name == physical_element.name){
                element.w = (float) physical_element.width;
                element.h = (float) physical_element.height;
            }
        }
    }
}

void claster(std::vector<element> &sample_elements){
    std::vector<element> divided_elements;

    for (element &sample_element : sample_elements){
        auto index = sample_elements.begin();

        if (sample_element.name != "io" && sample_element.name != "clb") {

            if ((sample_element.x.end_expr != none && sample_element.y.end_expr != none) ||
                (sample_element.x.end_expr != 0 && sample_element.y.end_expr != 0)) {

                // the number of elements which consists in x,y axes
                float length_x = (sample_element.x.end_expr - sample_element.x.start_expr - 1) / sample_element.w;
                float length_y = (sample_element.y.end_expr - sample_element.y.start_expr - 1) / sample_element.h;


                for (int i = 0; i < (int)length_y; i+=(int)sample_element.h){
                    for (int j = 0; j < (int)length_x; j+=(int)sample_element.w){
                        element created_element;

                        created_element = sample_element;
                        created_element.x.start_expr = sample_element.x.start_expr + (float)j;
                        created_element.y.start_expr = sample_element.y.start_expr + (float)i;

                        divided_elements.push_back(created_element);
                    }
                }

            }

            else if (sample_element.x.end_expr != none || sample_element.x.end_expr != 0 ) {

                // the amount of elements in claster
                float num_elements = (sample_element.x.end_expr - sample_element.x.start_expr - 1) / sample_element.w;


                for (int i = 0; i < (int)num_elements; i+=(int)sample_element.w){
                    element created_element;

                    created_element = sample_element;
                    created_element.x.start_expr = sample_element.x.start_expr + (float)i;

                    divided_elements.push_back(created_element);
                }

            }

            else if (sample_element.y.end_expr != none || sample_element.y.end_expr != 0 ) {

                // the amount of elements in claster
                float num_elements = (sample_element.y.end_expr - sample_element.y.start_expr - 1) / sample_element.h;


                for (int i = 0; i < (int)num_elements; i+=(int)sample_element.h){
                    element created_element;

                    created_element = sample_element;
                    created_element.y.start_expr = sample_element.y.start_expr + (float)i;

                    divided_elements.push_back(created_element);
                }
            }


        }
    }


    for (element &divided_element : divided_elements){
        auto index = sample_elements.begin();
        for (element &sample_element : sample_elements){
            if (sample_element.x.start_expr == divided_element.x.start_expr &&
                sample_element.y.start_expr == divided_element.y.start_expr &&
                sample_element.x.end_expr == divided_element.x.end_expr &&
                sample_element.y.end_expr == divided_element.y.end_expr){
                sample_elements.erase(index);
            }
            ++index;
        }
    }
    for (element &divided_element : divided_elements){
        sample_elements.push_back(divided_element);
    }

}

void repeating(t_arch &arch, std::vector<element> &sample_elements){
    int WIDTH = arch.grid_layouts[0].width;
    int HEIGHT = arch.grid_layouts[0].height;

    std::vector<element> temp;

    // adding the names of duplicate elements
    for (element &obj : sample_elements){
        if (obj.name != "io" && obj.name != "clb") {
            temp.push_back(obj);
        }
    }

    if (!temp.empty()){

        // finding the name of the element
        for (element &obj : temp) {
            std::vector<element> local_created_elements;

            if ((obj.x.repeat_expr != -1.0 && obj.x.repeat_expr != 0) &&
                (obj.y.repeat_expr != -1.0 && obj.y.repeat_expr != 0)){
                int start_x = (int) obj.x.start_expr-1;
                int repeat_x = (int) obj.x.repeat_expr;
                int end_x = (int) obj.x.end_expr;

                float difference_x = (float) (end_x - start_x);

                int count_x = (WIDTH-start_x)/repeat_x;
                int second_x = start_x + repeat_x;
                int end_point_x = start_x + repeat_x*count_x;
                if ((end_point_x+difference_x) >= (WIDTH-1))
                    end_point_x = start_x + repeat_x*(count_x-1);

                int start_y = (int) obj.y.start_expr - 1;
                int repeat_y = (int) obj.y.repeat_expr;
                int end_y = (int) obj.y.end_expr;

                float difference_y = (float) (end_y - start_y);

                int count_y = (HEIGHT-start_y)/repeat_y;
                int second_y = start_y + repeat_y;
                int end_point_y = start_y + repeat_y*count_y;
                if ((end_point_y+difference_y) >= (HEIGHT-1))
                    end_point_y = start_y + repeat_y*(count_y-1);


                for (int i = second_y; i <= end_point_y; i+=repeat_y) {
                    element temp_element_y_axis = obj;

                    temp_element_y_axis.y.start_expr = (float)i;
                    if (temp_element_y_axis.y.end_expr != 0 && temp_element_y_axis.x.end_expr != -1.0)
                        temp_element_y_axis.y.end_expr = (float)i + difference_x;

                    for (int j = second_x; j <= end_point_x; j += repeat_x) {
                        element temp_element_x_axis = temp_element_y_axis;
                        temp_element_x_axis.x.start_expr = (float)j;
                        if (temp_element_x_axis.x.end_expr != 0 && temp_element_x_axis.x.end_expr != -1.0)
                            temp_element_x_axis.x.end_expr = (float)j + difference_x;
                        local_created_elements.push_back(temp_element_x_axis);
                    }
                }
            }

            else if (obj.x.repeat_expr != -1.0 && obj.x.repeat_expr != 0){
                int start_x = (int) obj.x.start_expr - 1;
                int repeat_x = (int) obj.x.repeat_expr;
                int end_x = (int) obj.x.end_expr;

                float difference_x = (float) (end_x - start_x);

                int count_x = (WIDTH-start_x)/repeat_x;
                int second_point = start_x + repeat_x;
                int end_point = start_x + repeat_x*count_x;
                if ((end_point+difference_x) >= (WIDTH-1))
                    end_point = start_x + repeat_x*(count_x-1);


                for (int i = second_point; i <= end_point; i += repeat_x) {
                    element temp_element_x_axis = obj;
                    temp_element_x_axis.x.start_expr = (float)i;
                    if (temp_element_x_axis.x.end_expr != 0 && temp_element_x_axis.x.end_expr != -1.0)
                        temp_element_x_axis.x.end_expr = (float)i + difference_x;
                    local_created_elements.push_back(temp_element_x_axis);
                }

            }

            else if (obj.y.repeat_expr != -1.0 && obj.y.repeat_expr != 0){
                int start_y = (int) obj.y.start_expr - 1;
                int repeat_y = (int) obj.y.repeat_expr;
                int end_y = (int) obj.y.end_expr;

                float difference_y = (float) (end_y - start_y);

                int count_y = (HEIGHT-start_y)/repeat_y;
                int second_y = start_y + repeat_y;
                int end_point = start_y + repeat_y*count_y;
                if ((end_point+difference_y) >= (HEIGHT-1))
                    end_point = start_y + repeat_y*(count_y-1);


                for (int i = second_y; i <= end_point; i += repeat_y) {
                    element temp_element_y_axis = obj;
                    temp_element_y_axis.y.start_expr = (float)i;
                    if (temp_element_y_axis.x.end_expr != 0 && temp_element_y_axis.x.end_expr != -1.0)
                        temp_element_y_axis.x.end_expr = (float)i + difference_y;
                    local_created_elements.push_back(temp_element_y_axis);
                }
            }

            if(!local_created_elements.empty()){
                for (element &local_created_element : local_created_elements)
                    sample_elements.push_back(local_created_element);
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

void filling_clb(std::vector<element> &sample_elements, std::vector<std::vector<element>> &grid){
    element clb;
    std::vector<element> empty_elements;
    for (element& obj : sample_elements){
        if(obj.name == "clb"){
            clb = obj;
        }
    }

    for (std::vector<element> &raw : grid){
        for (element &obj : raw){
            obj = clb;
        }
    }


}

void filling_io(t_arch &arch, std::vector<element> &sample_elements, std::vector<std::vector<element>> &grid){
    int WIDTH = arch.grid_layouts[0].width;
    int HEIGHT = arch.grid_layouts[0].height;

    element none;
    element io;

    for (element& obj : sample_elements){
        if(obj.name == "io"){
            io = obj;
        }
    }

    for (size_t i = 0; i < size_t(HEIGHT) ; ++i ){
        for (size_t j = 0; j < size_t(WIDTH) ; ++j ){
            if (i==0 || (int) i == (HEIGHT-1) || j==0 || (int) j == (WIDTH-1))
                grid[i][j] = io;
        }
    }
}

void filling_empty(t_arch &arch, std::vector<element> &sample_elements, std::vector<std::vector<element>> &grid){
    int WIDTH = arch.grid_layouts[0].width;
    int HEIGHT = arch.grid_layouts[0].height;

    element none;
    std::vector<element> empty_elements;

    for (element& obj : sample_elements){
        if(obj.name == "EMPTY"){
            empty_elements.push_back(obj);
        }
    }

    for (size_t i = 0; i < HEIGHT ; ++i ){
        for (size_t j = 0; j < WIDTH ; ++j ){
            for (element& obj : empty_elements){
                if (obj.x.start_expr == 0 && obj.y.start_expr == 0)
                    grid[HEIGHT-1][0] = obj;
                else if (obj.x.start_expr == WIDTH && obj.y.start_expr == 0)
                    grid[HEIGHT-1][WIDTH-1] = obj;
                else if (obj.x.start_expr == 0 && obj.y.start_expr == HEIGHT)
                    grid[0][0] = obj;
                else if (obj.x.start_expr == WIDTH && obj.y.start_expr == HEIGHT)
                    grid[0][WIDTH-1] = obj;
                else
                    grid[HEIGHT-1-obj.y.start_expr][(size_t)obj.x.start_expr] = obj;
            }
        }
    }

}

void filling_others(t_arch &arch, std::vector<element> &sample_elements, std::vector<std::vector<element>> &grid){
    int WIDTH = arch.grid_layouts[0].width;
    int HEIGHT = arch.grid_layouts[0].height;

    element none;
    std::vector<element> other_elements;

    for (element& obj : sample_elements){
        if(obj.name != "EMPTY" && obj.name != "clb" && obj.name != "io"){
            other_elements.push_back(obj);
        }
    }

    for (size_t i = 0; i < HEIGHT ; ++i ){
        for (size_t j = 0; j < WIDTH ; ++j ){
            for (element& obj : other_elements){

                if ((float) j == obj.x.start_expr && i == (int) (HEIGHT-obj.y.start_expr)){
                    grid[i][j] = obj;
                    if (obj.h != 1){
                        for (size_t h=0; h < obj.h; ++h){
                            grid[i-h-1][j] = none; //+1
                            grid[i-h-1][j].name = obj.name;
                        }
                    }
                    else if (obj.w != 1){
                        for (size_t w=0; w < obj.w; ++w){
                            grid[i-1][j+w+1] = none;
                            grid[i-1][j+w+1].name = obj.name;
                        }
                    }
                    else if (obj.h != 1 && obj.w != 1) {

                        for (size_t h=0; h < obj.h; ++h){
                            for (size_t w=0; w < obj.w; ++w){
                                grid[i-h-1][j+w+1] = none;
                                grid[i-h-1][j+w+1].name = obj.name;
                            }
                        }

                    }


                }
            }
        }
    }


}

void assigning_coordinates (t_arch &arch, std::vector<element> &new_elements, std::vector<std::vector<element>> &grid){
    int WIDTH = arch.grid_layouts[0].width;
    int HEIGHT = arch.grid_layouts[0].height;

    for (size_t i = 0; i < HEIGHT ; ++i ){
        for (size_t j = 0; j < WIDTH ; ++j ){
            element temp;
            if (grid[i][j].x.start_expr != -1 && grid[i][j].y.start_expr !=-1){
                temp = grid[i][j];
                temp.x.start_expr = (float) j;
                temp.y.start_expr = (float) i;
//                temp.x.start_expr = (float) j/WIDTH;
//                temp.y.start_expr = (float) i/HEIGHT;
//                temp.w = (temp.w-0.2f)/WIDTH;
//                temp.h = (temp.h-0.2f)/HEIGHT;
                new_elements.push_back(temp);
            }
        }
    }
}

void set_id(std::vector<element> &new_elements){
    int id=0;
    for (element &obj : new_elements){
        obj.id = id;
        id+=1;
    }
}

void print_new_elements(std::vector<element> &new_elements){
    std::cout << "**************************************" << std::endl;
    std::cout << "-------------new_elements-------------" << std::endl;
    std::cout << "**************************************" << std::endl;
    for (const element &obj : new_elements){

        printf("  name: %s\n", obj.name.c_str());

        printf("\t_id: %d\n", obj.id);

        printf("\t_startx: %f\n", obj.x.start_expr);
        printf("\t_starty: %f\n", obj.y.start_expr);

        printf("\t_h: %f\n", obj.h);
        printf("\t_w: %f\n", obj.w);

    }

}

void print_grid(std::vector<std::vector<element>> &grid){
    for (std::vector<element> &raw : grid){
        for(element& obj : raw){
            std::cout << obj.y.start_expr << " " << obj.x.start_expr << "|";
        }
        std::cout << std::endl;
    }
}

void print_names_grid(std::vector<std::vector<element>> &grid){
    for (std::vector<element> &raw : grid){
        for(element& obj : raw){
            std::cout << obj.name << " | ";
        }
        std::cout << std::endl;
    }
}



void print_new_elements_grid (t_arch &arch, std::vector<element> &new_elements){
    int WIDTH = arch.grid_layouts[0].width;
    int HEIGHT = arch.grid_layouts[0].height;

    std::cout << "**************************************" << std::endl;
    std::cout << "----------new_elements_grid-----------" << std::endl;
    std::cout << "**************************************" << std::endl;
    int size = new_elements.size();
    for (size_t i=0; i < HEIGHT; ++i){
        for (size_t j=0; j < WIDTH; ++j){
            std::cout << new_elements[j+i*size].y.start_expr << " "
                      << new_elements[j+i*size].x.start_expr << "|";
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
    std::vector<element> new_elements;
    std::vector<std::vector<element>> grid;

    arch_to_vector(arch, sample_elements); //tested

    adding_wh(PhysicalTileTypes, sample_elements); //tested

    repeating(arch, sample_elements); //tested
    print_samples(sample_elements);

    claster(sample_elements); //tested
    print_samples(sample_elements);

    set_grid(arch, grid);
    std::cout << "*********************set_grid*************************" << std::endl;
    print_grid(grid);

    filling_clb(sample_elements, grid);
    std::cout << "*********************filling_clb*************************" << std::endl;
    print_grid(grid);

    filling_others(arch, sample_elements, grid);
    std::cout << "*********************filling_others*************************" << std::endl;
    print_grid(grid);

    filling_io(arch, sample_elements, grid);
    std::cout << "*********************filling_io*************************" << std::endl;
    print_grid(grid);

    filling_empty(arch, sample_elements, grid);
    std::cout << "*********************filling_empty*************************" << std::endl;
    print_grid(grid);



//    std::cout << "**********************************************" << std::endl;
//
//    assigning_coordinates(arch, new_elements, grid);
//    set_id(new_elements);
//
//    print_grid(grid);

    std::cout << "*********************RESULT*************************" << std::endl;
    print_names_grid(grid);


//    print_new_elements_grid(arch, new_elements);

//    print_new_elements(new_elements);
//    print_file_xml(new_elements);

    return 0;
}
