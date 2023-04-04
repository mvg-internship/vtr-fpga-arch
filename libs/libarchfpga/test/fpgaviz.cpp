#include <iostream>
#include "read_xml_arch_file.h"
#include "device_grid.h"


int main() {
    const char* arch_filename = "EArch.xml";

    // Load architecture from XML file
    t_arch arch;
    bool timing_enabled = true;
    std::vector<t_physical_tile_type> PhysicalTileTypes;
    std::vector<t_logical_block_type> LogicalBlockTypes;

    XmlReadArch(arch_filename, timing_enabled, &arch, PhysicalTileTypes, LogicalBlockTypes);

    
    std::cout << "**************************************" << std::endl;
    std::cout << "----------PhysicalTileTypes-----------" << std::endl;
    std::cout << "**************************************" << std::endl;
    for (t_physical_tile_type &obj : PhysicalTileTypes){
        printf("name: %s\n", obj.name);
        printf("\t_width: %d\n", obj.width);
        printf("\t_height: %d\n", obj.height);
    }


    return 0;
}
