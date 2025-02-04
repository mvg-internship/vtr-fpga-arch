/*
 * Copyright 2023 CAS—Atlantic (University of New Brunswick, CASA)
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string.h>
#include <math.h>
#include "odin_globals.h"
#include "odin_types.h"
#include "odin_util.h"
#include "ast_util.h"

#include "netlist_utils.h"
#include "node_creation_library.h"
#include "hard_blocks.h"
#include "memories.h"
#include "partial_map.h"
#include "vtr_util.h"
#include "vtr_memory.h"

using vtr::t_linked_vptr;

t_model* single_port_rams = NULL;
t_model* dual_port_rams = NULL;

t_linked_vptr* sp_memory_list;
t_linked_vptr* dp_memory_list;
t_linked_vptr* split_list;
t_linked_vptr* memory_instances = NULL;
t_linked_vptr* memory_port_size_list = NULL;

void copy_input_port_to_memory(nnode_t* node, signal_list_t* signals, const char* port_name);
void copy_output_port_to_memory(nnode_t* node, signal_list_t* signals, const char* port_name);
void pad_dp_memory_width(nnode_t* node, netlist_t* netlist);
void pad_sp_memory_width(nnode_t* node, netlist_t* netlist);
void pad_memory_output_port(nnode_t* node, netlist_t* netlist, t_model* model, const char* port_name);
void pad_memory_input_port(nnode_t* node, netlist_t* netlist, t_model* model, const char* port_name);

int get_sp_ram_split_width();
int get_dp_ram_split_width();
void filter_memories_by_soft_logic_cutoff();

/**
 * (function: init_sp_ram_signals)
 * 
 * @brief initialize sp ram signal lists
 */
sp_ram_signals* init_sp_ram_signals() {
    sp_ram_signals* signals = (sp_ram_signals*)vtr::malloc(sizeof(sp_ram_signals));

    signals->addr = init_signal_list();
    signals->data = init_signal_list();
    signals->out = init_signal_list();

    return (signals);
}

/**
 * (function: init_dp_ram_signals)
 * 
 * @brief initialize dp ram signal lists
 */
dp_ram_signals* init_dp_ram_signals() {
    dp_ram_signals* signals = (dp_ram_signals*)vtr::malloc(sizeof(dp_ram_signals));

    signals->addr1 = init_signal_list();
    signals->addr2 = init_signal_list();
    signals->data1 = init_signal_list();
    signals->data2 = init_signal_list();
    signals->out1 = init_signal_list();
    signals->out2 = init_signal_list();

    return (signals);
}

long get_sp_ram_depth(nnode_t* node) {
    sp_ram_signals* signals = get_sp_ram_signals(node);
    long depth = shift_left_value_with_overflow_check(0x1, signals->addr->count, node->loc);
    free_sp_ram_signals(signals);
    return depth;
}

long get_dp_ram_depth(nnode_t* node) {
    dp_ram_signals* signals = get_dp_ram_signals(node);
    oassert(signals->addr1->count == signals->addr2->count);
    long depth = shift_left_value_with_overflow_check(0x1, signals->addr1->count, node->loc);
    free_dp_ram_signals(signals);
    return depth;
}

long get_sp_ram_width(nnode_t* node) {
    sp_ram_signals* signals = get_sp_ram_signals(node);
    long width = signals->data->count;
    free_sp_ram_signals(signals);
    return width;
}

long get_dp_ram_width(nnode_t* node) {
    dp_ram_signals* signals = get_dp_ram_signals(node);
    oassert(signals->data1->count == signals->data2->count);
    long width = signals->data1->count;
    free_dp_ram_signals(signals);
    return width;
}

long get_memory_port_size(const char* name) {
    t_linked_vptr* mpl;

    mpl = memory_port_size_list;
    while (mpl != NULL) {
        if (strcmp(((t_memory_port_sizes*)mpl->data_vptr)->name, name) == 0)
            return ((t_memory_port_sizes*)mpl->data_vptr)->size;
        mpl = mpl->next;
    }
    return -1;
}

void copy_input_port_to_memory(nnode_t* node, signal_list_t* signalsvar, const char* port_name) {
    signal_list_t* temp = copy_input_signals(signalsvar);
    add_input_port_to_memory(node, temp, port_name);
    free_signal_list(temp);
}

void copy_output_port_to_memory(nnode_t* node, signal_list_t* signalsvar, const char* port_name) {
    signal_list_t* temp = copy_output_signals(signalsvar);
    add_output_port_to_memory(node, temp, port_name);
    free_signal_list(temp);
}

/*
 * Re-maps the given input signals to the given port name on the given memory node.
 */
void remap_input_port_to_memory(nnode_t* node, signal_list_t* signals, const char* port_name) {
    int i;
    int j = node->num_input_pins;

    // Make sure the port is not already assigned.
    for (i = 0; i < j; i++) {
        npin_t* pin = node->input_pins[i];
        if (!strcmp(pin->mapping, port_name)) {
            error_message(NETLIST, node->loc,
                          "Attempted to reassign output port %s to memory %s.", port_name, node->name);
        }
    }

    // Make room for the new port.
    allocate_more_input_pins(node, signals->count);
    add_input_port_information(node, signals->count);

    // Add the new port.
    for (i = 0; i < signals->count; i++, j++) {
        npin_t* pin = signals->pins[i];
        if (strcmp(pin->mapping, port_name)) {
            if (pin->mapping)
                vtr::free(pin->mapping);
            pin->mapping = vtr::strdup(port_name);
        }
        remap_pin_to_new_node(pin, node, j);
    }
}

/*
 * Adds an input port with the given name and signals to the given memory node.
 *
 * Only allows each port to be added once.
 */
void add_input_port_to_memory(nnode_t* node, signal_list_t* signalsvar, const char* port_name) {
    int i;
    int j = node->num_input_pins;

    // Make sure the port is not already assigned.
    for (i = 0; i < j; i++) {
        npin_t* pin = node->input_pins[i];
        if (!strcmp(pin->mapping, port_name)) {
            error_message(NETLIST, node->loc,
                          "Attempted to reassign input port %s to memory %s.", port_name, node->name);
        }
    }

    // Make room for the new port.
    allocate_more_input_pins(node, signalsvar->count);
    add_input_port_information(node, signalsvar->count);

    // Add the new port.
    for (i = 0; i < signalsvar->count; i++, j++) {
        npin_t* pin = signalsvar->pins[i];
        if (pin->mapping) {
            vtr::free(pin->mapping);
        }
        pin->mapping = vtr::strdup(port_name);
        add_input_pin_to_node(node, pin, j);
    }
}

/*
 * Re-maps the given output signals to the given port name on the given memory node.
 */
void remap_output_port_to_memory(nnode_t* node, signal_list_t* signalsvar, const char* port_name) {
    int i;
    int j = node->num_output_pins;

    // Make sure the port is not already assigned.
    for (i = 0; i < j; i++) {
        npin_t* pin = node->output_pins[i];
        if (!strcmp(pin->mapping, port_name)) {
            error_message(NETLIST, node->loc,
                          "Attempted to reassign output port %s to node %s.", port_name, node->name);
        }
    }

    // Make room for the new port.
    allocate_more_output_pins(node, signalsvar->count);
    add_output_port_information(node, signalsvar->count);

    // Add the new port.
    for (i = 0; i < signalsvar->count; i++, j++) {
        npin_t* pin = signalsvar->pins[i];
        if (strcmp(pin->mapping, port_name)) {
            if (pin->mapping)
                vtr::free(pin->mapping);
            pin->mapping = vtr::strdup(port_name);
        }
        remap_pin_to_new_node(pin, node, j);
    }
}

/*
 * Adds an output port with the given name and the given
 * signals to the given memory node. Only allows the same port
 * to be added once.
 */
void add_output_port_to_memory(nnode_t* node, signal_list_t* signals, const char* port_name) {
    int i;
    int j = node->num_output_pins;

    // Make sure the port is not already assigned.
    // TODO: more complicated logic needs to be implementd this is temporary solution
    for (i = 0; i < j; i++) {
        npin_t* pin = node->output_pins[i];
        if (!strcmp(pin->mapping, port_name)) {
            error_message(NETLIST, node->loc,
                          "Attempted to reassign output port %s to node %s.", port_name, node->name);
            return;
        }
    }

    // Make room for the new port.
    allocate_more_output_pins(node, signals->count);
    add_output_port_information(node, signals->count);

    // Add the new port.
    for (i = 0; i < signals->count; i++, j++) {
        npin_t* pin = signals->pins[i];
        if (pin->mapping) {
            vtr::free(pin->mapping);
        }
        pin->mapping = vtr::strdup(port_name);
        add_output_pin_to_node(node, pin, j);
    }
}

/*
 * Checks memories to ensure that they fall within sane size boundaries.
 *
 * Reports the memory distribution as well.
 */
void check_memories_and_report_distribution() {
    if ((sp_memory_list == NULL) && (dp_memory_list == NULL))
        return;

    printf("\nHard Logical Memory Distribution\n");
    printf("============================\n");

    long total_memory_bits = 0;
    int total_memory_block_counter = 0;
    long memory_max_width = 0;
    long memory_max_depth = 0;

    t_linked_vptr* temp = sp_memory_list;
    while (temp != NULL) {
        nnode_t* node = (nnode_t*)temp->data_vptr;

        long width = get_sp_ram_width(node);
        long depth = get_sp_ram_depth(node);

        if (depth > shift_left_value_with_overflow_check(0x1, HARD_RAM_ADDR_LIMIT, node->loc))
            error_message(NETLIST, node->loc, "Memory %s of depth %zu exceeds ODIN depth bound of 2^%d.", node->name, depth, HARD_RAM_ADDR_LIMIT);

        printf("SPRAM: %zu width %zu depth\n", width, depth);

        total_memory_bits += width * depth;

        total_memory_block_counter++;

        if (width > memory_max_width) {
            memory_max_width = width;
        }
        if (depth > memory_max_depth) {
            memory_max_depth = depth;
        }

        temp = temp->next;
    }

    temp = dp_memory_list;
    while (temp != NULL) {
        nnode_t* node = (nnode_t*)temp->data_vptr;

        long width = get_dp_ram_width(node);
        long depth = get_dp_ram_depth(node);
        if (depth > shift_left_value_with_overflow_check(0x1, HARD_RAM_ADDR_LIMIT, node->loc))
            error_message(NETLIST, node->loc, "Memory %s of depth %zu exceeds ODIN depth bound of 2^%d.", node->name, depth, HARD_RAM_ADDR_LIMIT);

        printf("DPRAM: %zu width %zu depth\n", width, depth);
        total_memory_bits += width * depth;

        total_memory_block_counter++;
        if (width > memory_max_width) {
            memory_max_width = width;
        }
        if (depth > memory_max_depth) {
            memory_max_depth = depth;
        }

        temp = temp->next;
    }

    printf("\nTotal Logical Memory Blocks = %d \n", total_memory_block_counter);
    printf("Total Logical Memory bits = %ld \n", total_memory_bits);
    printf("Max Memory Width = %ld \n", memory_max_width);
    printf("Max Memory Depth = %ld \n", memory_max_depth);
    printf("\n");

    return;
}

/*-------------------------------------------------------------------------
 * (function: split_sp_memory_depth)
 *
 * This function works to split the depth of a single port memory into
 *   several smaller memories.
 *
 *   split_size: the number of address bits in the resulting memory.
 *------------------------------------------------------------------------
 */
void split_sp_memory_depth(nnode_t* node, int split_size) {
    sp_ram_signals* signals = get_sp_ram_signals(node);

    int logical_size = signals->addr->count;

    /* Check that the memory needs to be split */
    if (logical_size <= split_size) {
        free_sp_ram_signals(signals);
        sp_memory_list = insert_in_vptr_list(sp_memory_list, node);
        return;
    }

    int i;
    signal_list_t* new_addr = init_signal_list();
    for (i = 1; i < signals->addr->count; i++)
        add_pin_to_signal_list(new_addr, signals->addr->pins[i]);

    /* Create the new memory node */
    nnode_t* new_mem_node1 = allocate_nnode(node->loc);
    nnode_t* new_mem_node2 = allocate_nnode(node->loc);

    // Append the new name with an __S or __H
    new_mem_node1->name = append_string(node->name, "__S");
    new_mem_node2->name = append_string(node->name, "__H");

    /* Copy properties from the original memory node */
    new_mem_node1->type = node->type;
    new_mem_node1->related_ast_node = node->related_ast_node;
    new_mem_node1->traverse_visited = node->traverse_visited;
    new_mem_node2->type = node->type;
    new_mem_node2->related_ast_node = node->related_ast_node;
    new_mem_node2->traverse_visited = node->traverse_visited;

    // Move over the original pins to the first memory node.
    signal_list_t* clk = init_signal_list();
    add_pin_to_signal_list(clk, signals->clk);
    remap_input_port_to_memory(new_mem_node1, new_addr, "addr");
    remap_input_port_to_memory(new_mem_node1, signals->data, "data");

    // Copy the inputs to the second memory node.
    copy_input_port_to_memory(new_mem_node2, new_addr, "addr");
    copy_input_port_to_memory(new_mem_node2, signals->data, "data");

    // Hook up addresses and write enables.
    {
        signal_list_t* we;
        nnode_t* and_g = make_2port_gate(LOGICAL_AND, 1, 1, 1, new_mem_node1, new_mem_node1->traverse_visited);
        remap_pin_to_new_node(signals->we, and_g, 1);
        remap_pin_to_new_node(signals->addr->pins[0], and_g, 0);

        we = make_output_pins_for_existing_node(and_g, 1);
        add_input_port_to_memory(new_mem_node1, we, "we");
        free_signal_list(we);

        nnode_t* not_g = make_not_gate_with_input(copy_input_npin(signals->addr->pins[0]), new_mem_node2, new_mem_node2->traverse_visited);
        and_g = make_2port_gate(LOGICAL_AND, 1, 1, 1, new_mem_node2, new_mem_node2->traverse_visited);
        connect_nodes(not_g, 0, and_g, 0);

        add_input_pin_to_node(and_g, copy_input_npin(signals->we), 1);

        we = make_output_pins_for_existing_node(and_g, 1);
        add_input_port_to_memory(new_mem_node2, we, "we");
        free_signal_list(we);
    }

    // Add the clock signals.
    remap_input_port_to_memory(new_mem_node1, clk, "clk");
    copy_input_port_to_memory(new_mem_node2, clk, "clk");
    free_signal_list(clk);

    // Setup output ports on both nodes.
    allocate_more_output_pins(new_mem_node1, signals->out->count);
    add_output_port_information(new_mem_node1, signals->out->count);

    allocate_more_output_pins(new_mem_node2, signals->out->count);
    add_output_port_information(new_mem_node2, signals->out->count);

    /* Copy over the output pins for the new memory */
    for (i = 0; i < signals->data->count; i++) {
        nnode_t* mux = make_2port_gate(MUX_2, 2, 2, 1, new_mem_node1, new_mem_node1->traverse_visited);
        nnode_t* not_g = make_not_gate(new_mem_node1, new_mem_node1->traverse_visited);
        add_input_pin_to_node(mux, copy_input_npin(signals->addr->pins[0]), 0);
        add_input_pin_to_node(not_g, copy_input_npin(signals->addr->pins[0]), 0);
        connect_nodes(not_g, 0, mux, 1);

        npin_t* pin = signals->out->pins[i];
        if (pin->name)
            vtr::free(pin->name);
        pin->name = mux->name;

        if (pin->mapping)
            vtr::free(pin->mapping);
        pin->mapping = NULL;

        remap_pin_to_new_node(pin, mux, 0);

        connect_nodes(new_mem_node1, i, mux, 2);
        if (new_mem_node1->output_pins[i]->mapping) {
            vtr::free(new_mem_node1->output_pins[i]->mapping);
        }
        new_mem_node1->output_pins[i]->mapping = vtr::strdup("out");

        connect_nodes(new_mem_node2, i, mux, 3);
        if (new_mem_node2->output_pins[i]->mapping) {
            vtr::free(new_mem_node2->output_pins[i]->mapping);
        }
        new_mem_node2->output_pins[i]->mapping = vtr::strdup("out");
    }

    free_sp_ram_signals(signals);
    free_signal_list(new_addr);

    free_nnode(node);

    split_sp_memory_depth(new_mem_node1, split_size);
    split_sp_memory_depth(new_mem_node2, split_size);
}

/*-------------------------------------------------------------------------
 * (function: split_dp_memory_depth)
 *
 * This function works to split the depth of a dual port memory into
 *   several smaller memories.
 *------------------------------------------------------------------------
 */
void split_dp_memory_depth(nnode_t* node, int split_size) {
    dp_ram_signals* signals = get_dp_ram_signals(node);

    int logical_size = signals->addr1->count;

    /* Check that the memory needs to be split */
    if (logical_size <= split_size) {
        free_dp_ram_signals(signals);
        dp_memory_list = insert_in_vptr_list(dp_memory_list, node);
        return;
    }

    signal_list_t* new_addr1 = init_signal_list();

    int i;
    for (i = 1; i < signals->addr1->count; i++)
        add_pin_to_signal_list(new_addr1, signals->addr1->pins[i]);

    signal_list_t* new_addr2 = init_signal_list();
    for (i = 1; i < signals->addr2->count; i++)
        add_pin_to_signal_list(new_addr2, signals->addr2->pins[i]);

    /* Create the new memory node */
    nnode_t* new_mem_node1 = allocate_nnode(node->loc);
    nnode_t* new_mem_node2 = allocate_nnode(node->loc);

    // Append the new name with an __S or __H
    new_mem_node1->name = append_string(node->name, "__S");
    new_mem_node2->name = append_string(node->name, "__H");

    /* Copy properties from the original memory node */
    new_mem_node1->type = node->type;
    new_mem_node1->related_ast_node = node->related_ast_node;
    new_mem_node1->traverse_visited = node->traverse_visited;
    new_mem_node2->type = node->type;
    new_mem_node2->related_ast_node = node->related_ast_node;
    new_mem_node2->traverse_visited = node->traverse_visited;

    // Move over the original pins to the first memory node.
    signal_list_t* clk = init_signal_list();
    add_pin_to_signal_list(clk, signals->clk);
    remap_input_port_to_memory(new_mem_node1, new_addr1, "addr1");
    remap_input_port_to_memory(new_mem_node1, new_addr2, "addr2");
    remap_input_port_to_memory(new_mem_node1, signals->data1, "data1");
    remap_input_port_to_memory(new_mem_node1, signals->data2, "data2");

    // Copy the inputs to the second memory node.
    copy_input_port_to_memory(new_mem_node2, new_addr1, "addr1");
    copy_input_port_to_memory(new_mem_node2, new_addr2, "addr2");
    copy_input_port_to_memory(new_mem_node2, signals->data1, "data1");
    copy_input_port_to_memory(new_mem_node2, signals->data2, "data2");

    // Hook up addresses and write enables.
    {
        signal_list_t* we;
        nnode_t* and_node = make_2port_gate(LOGICAL_AND, 1, 1, 1, new_mem_node1, new_mem_node1->traverse_visited);
        remap_pin_to_new_node(signals->we1, and_node, 1);
        remap_pin_to_new_node(signals->addr1->pins[0], and_node, 0);

        we = make_output_pins_for_existing_node(and_node, 1);
        add_input_port_to_memory(new_mem_node1, we, "we1");
        free_signal_list(we);

        and_node = make_2port_gate(LOGICAL_AND, 1, 1, 1, new_mem_node1, new_mem_node1->traverse_visited);
        remap_pin_to_new_node(signals->we2, and_node, 1);
        remap_pin_to_new_node(signals->addr2->pins[0], and_node, 0);

        we = make_output_pins_for_existing_node(and_node, 1);
        add_input_port_to_memory(new_mem_node1, we, "we2");
        free_signal_list(we);

        nnode_t* not_g = make_not_gate_with_input(copy_input_npin(signals->addr1->pins[0]), new_mem_node2, new_mem_node2->traverse_visited);
        and_node = make_2port_gate(LOGICAL_AND, 1, 1, 1, new_mem_node2, new_mem_node2->traverse_visited);
        connect_nodes(not_g, 0, and_node, 0);
        add_input_pin_to_node(and_node, copy_input_npin(signals->we1), 1);

        we = make_output_pins_for_existing_node(and_node, 1);
        add_input_port_to_memory(new_mem_node2, we, "we1");
        free_signal_list(we);

        not_g = make_not_gate_with_input(copy_input_npin(signals->addr2->pins[0]), new_mem_node2, new_mem_node2->traverse_visited);
        and_node = make_2port_gate(LOGICAL_AND, 1, 1, 1, new_mem_node2, new_mem_node2->traverse_visited);
        connect_nodes(not_g, 0, and_node, 0);

        add_input_pin_to_node(and_node, copy_input_npin(signals->we2), 1);

        we = make_output_pins_for_existing_node(and_node, 1);
        add_input_port_to_memory(new_mem_node2, we, "we2");
        free_signal_list(we);
    }

    // Add the clock signals.
    remap_input_port_to_memory(new_mem_node1, clk, "clk");
    copy_input_port_to_memory(new_mem_node2, clk, "clk");
    free_signal_list(clk);

    // Setup output ports on both nodes.
    allocate_more_output_pins(new_mem_node1, signals->out1->count + signals->out2->count);
    add_output_port_information(new_mem_node1, signals->out1->count);
    add_output_port_information(new_mem_node1, signals->out2->count);

    allocate_more_output_pins(new_mem_node2, signals->out1->count + signals->out2->count);
    add_output_port_information(new_mem_node2, signals->out1->count);
    add_output_port_information(new_mem_node2, signals->out2->count);

    /* Copy over the output pins for the new memory */
    for (i = 0; i < signals->data1->count; i++) {
        nnode_t* mux = make_2port_gate(MUX_2, 2, 2, 1, new_mem_node1, new_mem_node1->traverse_visited);
        nnode_t* not_g = make_not_gate(new_mem_node1, new_mem_node1->traverse_visited);
        add_input_pin_to_node(mux, copy_input_npin(signals->addr1->pins[0]), 0);
        add_input_pin_to_node(not_g, copy_input_npin(signals->addr1->pins[0]), 0);
        connect_nodes(not_g, 0, mux, 1);

        npin_t* pin = signals->out1->pins[i];
        if (pin->name) {
            vtr::free(pin->name);
        }
        pin->name = mux->name;
        if (pin->mapping) {
            vtr::free(pin->mapping);
        }
        pin->mapping = NULL;

        remap_pin_to_new_node(pin, mux, 0);

        connect_nodes(new_mem_node1, i, mux, 2);
        if (new_mem_node1->output_pins[i]->mapping) {
            vtr::free(new_mem_node1->output_pins[i]->mapping);
        }
        new_mem_node1->output_pins[i]->mapping = vtr::strdup("out1");

        connect_nodes(new_mem_node2, i, mux, 3);
        if (new_mem_node2->output_pins[i]->mapping) {
            vtr::free(new_mem_node2->output_pins[i]->mapping);
        }
        new_mem_node2->output_pins[i]->mapping = vtr::strdup("out1");
    }

    /* Copy over the output pins for the new memory */
    for (i = 0; i < signals->data1->count; i++) {
        nnode_t* mux = make_2port_gate(MUX_2, 2, 2, 1, new_mem_node1, new_mem_node1->traverse_visited);
        nnode_t* not_g = make_not_gate(new_mem_node1, new_mem_node1->traverse_visited);
        add_input_pin_to_node(mux, copy_input_npin(signals->addr2->pins[0]), 0);
        add_input_pin_to_node(not_g, copy_input_npin(signals->addr2->pins[0]), 0);
        connect_nodes(not_g, 0, mux, 1);

        int pin_index = new_mem_node1->output_port_sizes[0] + i;

        npin_t* pin = signals->out2->pins[i];
        if (pin->name) {
            vtr::free(pin->name);
        }
        pin->name = mux->name;
        if (pin->mapping) {
            vtr::free(pin->mapping);
        }
        pin->mapping = NULL;

        remap_pin_to_new_node(pin, mux, 0);

        connect_nodes(new_mem_node1, pin_index, mux, 2);
        if (new_mem_node1->output_pins[pin_index]->mapping) {
            vtr::free(new_mem_node1->output_pins[pin_index]->mapping);
        }
        new_mem_node1->output_pins[pin_index]->mapping = vtr::strdup("out2");

        connect_nodes(new_mem_node2, pin_index, mux, 3);
        if (new_mem_node2->output_pins[pin_index]->mapping) {
            vtr::free(new_mem_node2->output_pins[pin_index]->mapping);
        }
        new_mem_node2->output_pins[pin_index]->mapping = vtr::strdup("out2");
    }

    free_dp_ram_signals(signals);
    free_signal_list(new_addr1);
    free_signal_list(new_addr2);
    free_nnode(node);

    split_dp_memory_depth(new_mem_node1, split_size);
    split_dp_memory_depth(new_mem_node2, split_size);
}

/*
 * Width-splits the given memory up into chunks the of the
 * width specified in the arch file.
 */
void split_sp_memory_width(nnode_t* node, int target_size) {
    char port_name[] = "data";
    int data_port_number = get_input_port_index_from_mapping(node, port_name);

    oassert(data_port_number != -1);

    int data_port_size = node->input_port_sizes[data_port_number];

    int num_memories = ceil((double)data_port_size / (double)target_size);

    if (data_port_size <= target_size) {
        // If we don't need to split, put the original node back.
        sp_memory_list = insert_in_vptr_list(sp_memory_list, node);
    } else {
        int i;
        int data_pins_moved = 0;
        int output_pins_moved = 0;
        for (i = 0; i < num_memories; i++) {
            nnode_t* new_node = allocate_nnode(node->loc);
            new_node->name = append_string(node->name, "-%d", i);
            sp_memory_list = insert_in_vptr_list(sp_memory_list, new_node);

            /* Copy properties from the original node */
            new_node->type = node->type;
            new_node->related_ast_node = node->related_ast_node;
            new_node->traverse_visited = node->traverse_visited;
            new_node->node_data = NULL;

            int j;
            for (j = 0; j < node->num_input_port_sizes; j++)
                add_input_port_information(new_node, 0);

            add_output_port_information(new_node, 0);

            int index = 0;
            int old_index = 0;
            for (j = 0; j < node->num_input_port_sizes; j++) {
                // Move this node's share of data pins out of the data port of the original node.
                if (j == data_port_number) {
                    // Skip over data pins we've already moved.
                    old_index += data_pins_moved;
                    int k;
                    for (k = 0; k < target_size && data_pins_moved < data_port_size; k++) {
                        allocate_more_input_pins(new_node, 1);
                        new_node->input_port_sizes[j]++;
                        remap_pin_to_new_node(node->input_pins[old_index], new_node, index);
                        index++;
                        old_index++;
                        data_pins_moved++;
                    }
                    int remaining_data_pins = data_port_size - data_pins_moved;
                    // Skip over pins we have yet to copy.
                    old_index += remaining_data_pins;
                } else {
                    int k;
                    for (k = 0; k < node->input_port_sizes[j]; k++) {
                        allocate_more_input_pins(new_node, 1);
                        new_node->input_port_sizes[j]++;
                        // Copy pins for all but the last memory. the last one get the original pins moved to it.
                        if (i < num_memories - 1)
                            add_input_pin_to_node(new_node, copy_input_npin(node->input_pins[old_index]), index);
                        else
                            remap_pin_to_new_node(node->input_pins[old_index], new_node, index);
                        index++;
                        old_index++;
                    }
                }
            }

            index = 0;
            old_index = 0;
            old_index += output_pins_moved;

            int k;
            for (k = 0; k < target_size && output_pins_moved < data_port_size; k++) {
                allocate_more_output_pins(new_node, 1);
                new_node->output_port_sizes[0]++;
                remap_pin_to_new_node(node->output_pins[old_index], new_node, index);
                index++;
                old_index++;
                output_pins_moved++;
            }
        }
        // Free the original node.
        free_nnode(node);
    }
}

/*
 * Splits the given dual port memory width into one or more memories with
 * width less than or equal to target_size.
 */
void split_dp_memory_width(nnode_t* node, int target_size) {
    char data1_name[] = "data1";
    char data2_name[] = "data2";
    char out1_name[] = "out1";
    char out2_name[] = "out2";

    int data1_port_number = get_input_port_index_from_mapping(node, data1_name);
    int data2_port_number = get_input_port_index_from_mapping(node, data2_name);

    int out1_port_number = get_output_port_index_from_mapping(node, out1_name);
    int out2_port_number = get_output_port_index_from_mapping(node, out2_name);

    oassert(data1_port_number != -1);
    oassert(data2_port_number != -1);
    oassert(out1_port_number != -1);
    oassert(out2_port_number != -1);

    int data1_port_size = node->input_port_sizes[data1_port_number];
    int data2_port_size = node->input_port_sizes[data2_port_number];

    int out1_port_size = node->output_port_sizes[out1_port_number];
    int out2_port_size = node->output_port_sizes[out2_port_number];

    oassert(data1_port_size == data2_port_size);
    oassert(out1_port_size == out2_port_size);
    oassert(data1_port_size == out1_port_size);

    int num_memories = ceil((double)data1_port_size / (double)target_size);

    if (data1_port_size <= target_size) {
        // If we're not splitting, put the original memory node back.
        dp_memory_list = insert_in_vptr_list(dp_memory_list, node);
    } else {
        int i;
        int data1_pins_moved = 0;
        int data2_pins_moved = 0;
        int out1_pins_moved = 0;
        int out2_pins_moved = 0;
        for (i = 0; i < num_memories; i++) {
            nnode_t* new_node = allocate_nnode(node->loc);
            new_node->name = append_string(node->name, "-%d", i);
            dp_memory_list = insert_in_vptr_list(dp_memory_list, new_node);

            /* Copy properties from the original node */
            new_node->type = node->type;
            new_node->related_ast_node = node->related_ast_node;
            new_node->traverse_visited = node->traverse_visited;
            new_node->node_data = NULL;

            int j;
            for (j = 0; j < node->num_input_port_sizes; j++)
                add_input_port_information(new_node, 0);

            int index = 0;
            int old_index = 0;
            for (j = 0; j < node->num_input_port_sizes; j++) {
                // Move this node's share of data pins out of the data port of the original node.
                if (j == data1_port_number) {
                    // Skip over data pins we've already moved.
                    old_index += data1_pins_moved;
                    int k;
                    for (k = 0; k < target_size && data1_pins_moved < data1_port_size; k++) {
                        allocate_more_input_pins(new_node, 1);
                        new_node->input_port_sizes[j]++;
                        remap_pin_to_new_node(node->input_pins[old_index], new_node, index);
                        index++;
                        old_index++;
                        data1_pins_moved++;
                    }
                    int remaining_data_pins = data1_port_size - data1_pins_moved;
                    // Skip over pins we have yet to copy.
                    old_index += remaining_data_pins;
                } else if (j == data2_port_number) {
                    // Skip over data pins we've already moved.
                    old_index += data2_pins_moved;
                    int k;
                    for (k = 0; k < target_size && data2_pins_moved < data2_port_size; k++) {
                        allocate_more_input_pins(new_node, 1);
                        new_node->input_port_sizes[j]++;
                        remap_pin_to_new_node(node->input_pins[old_index], new_node, index);
                        index++;
                        old_index++;
                        data2_pins_moved++;
                    }
                    int remaining_data_pins = data2_port_size - data2_pins_moved;
                    // Skip over pins we have yet to copy.
                    old_index += remaining_data_pins;
                } else {
                    int k;
                    for (k = 0; k < node->input_port_sizes[j]; k++) {
                        allocate_more_input_pins(new_node, 1);
                        new_node->input_port_sizes[j]++;
                        // Copy pins for all but the last memory. the last one get the original pins moved to it.
                        if (i < num_memories - 1)
                            add_input_pin_to_node(new_node, copy_input_npin(node->input_pins[old_index]), index);
                        else
                            remap_pin_to_new_node(node->input_pins[old_index], new_node, index);
                        index++;
                        old_index++;
                    }
                }
            }

            for (j = 0; j < node->num_output_port_sizes; j++)
                add_output_port_information(new_node, 0);

            index = 0;
            old_index = 0;
            for (j = 0; j < node->num_output_port_sizes; j++) {
                // Move this node's share of data pins out of the data port of the original node.
                if (j == out1_port_number) {
                    // Skip over data pins we've already moved.
                    old_index += out1_pins_moved;
                    int k;
                    for (k = 0; k < target_size && out1_pins_moved < out1_port_size; k++) {
                        allocate_more_output_pins(new_node, 1);
                        new_node->output_port_sizes[j]++;
                        remap_pin_to_new_node(node->output_pins[old_index], new_node, index);
                        index++;
                        old_index++;
                        out1_pins_moved++;
                    }
                    int remaining_pins = out1_port_size - out1_pins_moved;
                    // Skip over pins we have yet to copy.
                    old_index += remaining_pins;
                } else if (j == out2_port_number) {
                    // Skip over data pins we've already moved.
                    old_index += out2_pins_moved;
                    int k;
                    for (k = 0; k < target_size && out2_pins_moved < out2_port_size; k++) {
                        allocate_more_output_pins(new_node, 1);
                        new_node->output_port_sizes[j]++;
                        remap_pin_to_new_node(node->output_pins[old_index], new_node, index);
                        index++;
                        old_index++;
                        out2_pins_moved++;
                    }
                    int remaining_pins = out2_port_size - out2_pins_moved;
                    // Skip over pins we have yet to copy.
                    old_index += remaining_pins;
                } else {
                    oassert(false);
                }
            }
        }
        // Free the original node.
        free_nnode(node);
    }
}

/*
 * Determines the single port ram split depth based on the configuration
 * variables and architecture.
 */
long get_sp_ram_split_depth() {
    t_model_ports* hb_ports = get_model_port(single_port_rams->inputs, "addr");
    long split_size;
    if (configuration.split_memory_depth == -1) /* MIN */
        split_size = hb_ports->min_size;
    else if (configuration.split_memory_depth == -2) /* MIN */
        split_size = hb_ports->size;
    else if (configuration.split_memory_depth > 0)
        split_size = configuration.split_memory_depth;
    else
        split_size = hb_ports->size;

    oassert(split_size > 0);

    return split_size;
}

/*
 * Determines the dual port ram split depth based on the configuration
 * variables and architecture.
 */
long get_dp_ram_split_depth() {
    t_model_ports* hb_ports = get_model_port(dual_port_rams->inputs, "addr1");
    long split_depth;
    if (configuration.split_memory_depth == -1) /* MIN */
        split_depth = hb_ports->min_size;
    else if (configuration.split_memory_depth == -2) /* MIN */
        split_depth = hb_ports->size;
    else if (configuration.split_memory_depth > 0)
        split_depth = configuration.split_memory_depth;
    else
        split_depth = hb_ports->size;

    oassert(split_depth > 0);

    return split_depth;
}

/*
 * Determines the single port ram split depth based on the configuration
 * variables and architecture.
 */
int get_sp_ram_split_width() {
    if (configuration.split_memory_width) {
        return 1;
    } else {
        t_model* model = single_port_rams;
        char port_name[] = "data";
        t_model_ports* ports = get_model_port(model->inputs, port_name);
        return ports->size;
    }
}

/*
 * Determines the dual port ram split depth based on the configuration
 * variables and architecture.
 */
int get_dp_ram_split_width() {
    if (configuration.split_memory_width) {
        return 1;
    } else {
        t_model* model = dual_port_rams;
        char port_name[] = "data1";
        t_model_ports* ports = get_model_port(model->inputs, port_name);
        return ports->size;
    }
}

/*
 * Removes all memories from the sp_memory_list and dp_memory_list which do not
 * have more than configuration.soft_logic_memory_depth_threshold address bits.
 */
void filter_memories_by_soft_logic_cutoff() {
    if (single_port_rams) {
        t_linked_vptr* temp = sp_memory_list;
        sp_memory_list = NULL;
        while (temp != NULL) {
            nnode_t* node = (nnode_t*)temp->data_vptr;
            oassert(node != NULL);
            oassert(node->type == MEMORY);
            temp = delete_in_vptr_list(temp);

            long depth = get_sp_ram_depth(node);
            long width = get_sp_ram_width(node);
            if (depth > configuration.soft_logic_memory_depth_threshold || width > configuration.soft_logic_memory_width_threshold)
                sp_memory_list = insert_in_vptr_list(sp_memory_list, node);
        }
    }

    if (dual_port_rams) {
        t_linked_vptr* temp = dp_memory_list;
        dp_memory_list = NULL;
        while (temp != NULL) {
            nnode_t* node = (nnode_t*)temp->data_vptr;
            oassert(node != NULL);
            oassert(node->type == MEMORY);
            temp = delete_in_vptr_list(temp);

            long depth = get_dp_ram_depth(node);
            long width = get_dp_ram_width(node);
            if (depth > configuration.soft_logic_memory_depth_threshold || width > configuration.soft_logic_memory_width_threshold)
                dp_memory_list = insert_in_vptr_list(dp_memory_list, node);
        }
    }
}

/*-------------------------------------------------------------------------
 * (function: iterate_memories)
 *
 * This function will iterate over all of the memory hard blocks that
 *      exist in the netlist and perform a splitting so that they can
 *      be easily packed into hard memory blocks on the FPGA.
 *
 * This function will drop memories which fall below the soft logic threshold,
 * if those configuration variables are set.
 *-----------------------------------------------------------------------*/
void iterate_memories(netlist_t* netlist) {
    /* Report on Logical Memory usage */
    check_memories_and_report_distribution();

    // Remove memories that don't meet the soft logic cutoff.
    filter_memories_by_soft_logic_cutoff();

    if (single_port_rams) {
        // Depth split
        int split_depth = get_sp_ram_split_depth();
        t_linked_vptr* temp = sp_memory_list;
        sp_memory_list = NULL;
        while (temp != NULL) {
            nnode_t* node = (nnode_t*)temp->data_vptr;
            oassert(node != NULL);
            oassert(node->type == MEMORY);
            temp = delete_in_vptr_list(temp);
            split_sp_memory_depth(node, split_depth);
        }

        // Width split
        int split_width = get_sp_ram_split_width();
        temp = sp_memory_list;
        sp_memory_list = NULL;
        while (temp != NULL) {
            nnode_t* node = (nnode_t*)temp->data_vptr;
            oassert(node != NULL);
            oassert(node->type == MEMORY);
            temp = delete_in_vptr_list(temp);
            split_sp_memory_width(node, split_width);
        }

        // Remove memories that are too small to use hard blocks.
        filter_memories_by_soft_logic_cutoff();

        // Pad the rest.
        temp = sp_memory_list;
        sp_memory_list = NULL;
        while (temp != NULL) {
            nnode_t* node = (nnode_t*)temp->data_vptr;
            oassert(node != NULL);
            oassert(node->type == MEMORY);
            temp = delete_in_vptr_list(temp);
            pad_sp_memory_width(node, netlist);
            pad_memory_input_port(node, netlist, single_port_rams, "addr");
        }
    }

    if (dual_port_rams) {
        // Depth split
        int split_depth = get_dp_ram_split_depth();
        t_linked_vptr* temp = dp_memory_list;
        dp_memory_list = NULL;
        while (temp != NULL) {
            nnode_t* node = (nnode_t*)temp->data_vptr;
            oassert(node != NULL);
            oassert(node->type == MEMORY);
            temp = delete_in_vptr_list(temp);
            split_dp_memory_depth(node, split_depth);
        }

        // Width split
        int split_width = get_dp_ram_split_width();
        temp = dp_memory_list;
        dp_memory_list = NULL;
        while (temp != NULL) {
            nnode_t* node = (nnode_t*)temp->data_vptr;
            oassert(node != NULL);
            oassert(node->type == MEMORY);
            temp = delete_in_vptr_list(temp);
            split_dp_memory_width(node, split_width);
        }

        // Remove memories that are too small to use hard blocks.
        filter_memories_by_soft_logic_cutoff();

        // Pad the rest
        temp = dp_memory_list;
        dp_memory_list = NULL;
        while (temp != NULL) {
            nnode_t* node = (nnode_t*)temp->data_vptr;
            oassert(node != NULL);
            oassert(node->type == MEMORY);
            temp = delete_in_vptr_list(temp);
            pad_dp_memory_width(node, netlist);
            pad_memory_input_port(node, netlist, dual_port_rams, "addr1");
            pad_memory_input_port(node, netlist, dual_port_rams, "addr2");
        }
    }
}

/*-------------------------------------------------------------------------
 * (function: free_memory_lists)
 *
 * Clean up the memory by deleting the list structure of memories
 *      during optimisation.
 *-----------------------------------------------------------------------*/
void free_memory_lists() {
    while (sp_memory_list != NULL)
        sp_memory_list = delete_in_vptr_list(sp_memory_list);
    while (dp_memory_list != NULL)
        dp_memory_list = delete_in_vptr_list(dp_memory_list);
}

/*
 * Pads the width of a dual port memory to that specified in the arch file.
 */
void pad_dp_memory_width(nnode_t* node, netlist_t* netlist) {
    oassert(node->type == MEMORY);
    oassert(dual_port_rams != NULL);

    pad_memory_input_port(node, netlist, dual_port_rams, "data1");
    pad_memory_input_port(node, netlist, dual_port_rams, "data2");

    pad_memory_output_port(node, netlist, dual_port_rams, "out1");
    pad_memory_output_port(node, netlist, dual_port_rams, "out2");

    dp_memory_list = insert_in_vptr_list(dp_memory_list, node);
}

/*
 * Pads the width of a single port memory to that specified in the arch file.
 */
void pad_sp_memory_width(nnode_t* node, netlist_t* netlist) {
    oassert(node->type == MEMORY);
    oassert(single_port_rams != NULL);

    pad_memory_input_port(node, netlist, single_port_rams, "data");

    pad_memory_output_port(node, netlist, single_port_rams, "out");

    sp_memory_list = insert_in_vptr_list(sp_memory_list, node);
}

/*
 * Pads the given output port to the width specified in the given model.
 */
void pad_memory_output_port(nnode_t* node, netlist_t* /*netlist*/, t_model* model, const char* port_name) {
    static int pad_pin_number = 0;

    int port_number = get_output_port_index_from_mapping(node, port_name);
    int port_index = get_output_pin_index_from_mapping(node, port_name);

    int port_size = node->output_port_sizes[port_number];

    t_model_ports* ports = get_model_port(model->outputs, port_name);

    oassert(ports != NULL);

    int target_size = ports->size;
    int diff = target_size - port_size;

    if (diff > 0) {
        allocate_more_output_pins(node, diff);

        // Shift other pins to the right, if any.
        int i;
        for (i = node->num_output_pins - 1; i >= port_index + target_size; i--)
            move_output_pin(node, i - diff, i);

        for (i = port_index + port_size; i < port_index + target_size; i++) {
            // Add new pins to the higher order spots.
            npin_t* new_pin = allocate_npin();
            // Pad outputs with a unique and descriptive name to avoid collisions.
            new_pin->name = append_string("", "unconnected_memory_output~%d", pad_pin_number++);
            new_pin->mapping = vtr::strdup(port_name);
            add_output_pin_to_node(node, new_pin, i);
        }
        node->output_port_sizes[port_number] = target_size;
    }
}

/*
 * Pads the given input port to the width specified in the given model.
 */
void pad_memory_input_port(nnode_t* node, netlist_t* netlist, t_model* model, const char* port_name) {
    oassert(node->type == MEMORY);
    oassert(model != NULL);

    int port_number = get_input_port_index_from_mapping(node, port_name);
    int port_index = get_input_pin_index_from_mapping(node, port_name);

    oassert(port_number != -1);
    oassert(port_index != -1);

    int port_size = node->input_port_sizes[port_number];

    t_model_ports* ports = get_model_port(model->inputs, port_name);

    oassert(ports != NULL);

    int target_size = ports->size;
    int diff = target_size - port_size;

    // Expand the inputs
    if (diff > 0) {
        allocate_more_input_pins(node, diff);

        // Shift other pins to the right, if any.
        int i;
        for (i = node->num_input_pins - 1; i >= port_index + target_size; i--)
            move_input_pin(node, i - diff, i);

        for (i = port_index + port_size; i < port_index + target_size; i++) {
            add_input_pin_to_node(node, get_pad_pin(netlist), i);
            if (node->input_pins[i]->mapping) {
                vtr::free(node->input_pins[i]->mapping);
            }
            node->input_pins[i]->mapping = vtr::strdup(port_name);
        }

        node->input_port_sizes[port_number] = target_size;
    }
}

bool is_sp_ram(nnode_t* node) {
    oassert(node != NULL);
    oassert(node->type == MEMORY);
    return !strcmp(node->related_ast_node->identifier_node->types.identifier, SINGLE_PORT_RAM_string);
}

bool is_dp_ram(nnode_t* node) {
    oassert(node != NULL);
    oassert(node->type == MEMORY);
    return !strcmp(node->related_ast_node->identifier_node->types.identifier, DUAL_PORT_RAM_string);
}

/**
 * (function: is_blif_sp_ram)
 * 
 * @brief to check if the given node is a valid
 * single port ram based on VTR primitive definition
 * 
 * @param node pointing to a spram node 
 */
bool is_blif_sp_ram(nnode_t* node) {
    oassert(node->name);
    /* return value */
    bool is_ram = true;
    if (std::string(node->name).find(SINGLE_PORT_RAM_string) == std::string::npos)
        return false;

    /* check the num input/output ports */
    is_ram = (node->num_input_port_sizes == 4) && (node->num_output_port_sizes == 1);

    /* check if it is a ram */
    if (is_ram) {
        /* port connections were passed by name; verify input port names */
        for (int i = 0; i < node->num_input_pins && is_ram; i++) {
            oassert(node->input_pins[i]->mapping);
            char* port_id = node->input_pins[i]->mapping;

            /* comparision of mapping ports with single_port_ram port names */
            if ((strcmp(port_id, "we") != 0) && (strcmp(port_id, "clk") != 0) && (strcmp(port_id, "addr") != 0) && (strcmp(port_id, "data") != 0)) {
                is_ram = false;
                break;
            }
        }
        /* port connections were passed by name; verify output port names */
        for (int i = 0; i < node->num_output_pins && is_ram; i++) {
            oassert(node->output_pins[i]->mapping);
            char* port_id = node->output_pins[i]->mapping;

            /* comparision of mapping ports with single_port_ram port names */
            if ((strcmp(port_id, "out") != 0)) {
                is_ram = false;
                break;
            }
        }
    }

    return (is_ram);
}

/**
 * (function: is_blif_dp_ram)
 * 
 * @brief to check if the given node is a valid
 * dual port ram based on VTR primitive definition
 *  
 * @param node pointing to a dpram node 
 */
bool is_blif_dp_ram(nnode_t* node) {
    oassert(node->name);
    /* return value */
    bool is_ram = true;
    if (std::string(node->name).find(DUAL_PORT_RAM_string) == std::string::npos)
        return false;

    /* check the num input/output ports */
    is_ram = (node->num_input_port_sizes == 7) && (node->num_output_port_sizes == 2);

    /* check if it is a ram */
    if (is_ram) {
        /* port connections were passed by name; verify input port names */
        for (int i = 0; i < node->num_input_pins && is_ram; i++) {
            oassert(node->input_pins[i]->mapping);
            char* port_id = node->input_pins[i]->mapping;

            /* comparision of mapping ports with dual_port_ram port names */
            if ((strcmp(port_id, "clk") != 0) && (strcmp(port_id, "we1") != 0) && (strcmp(port_id, "we2") != 0) && (strcmp(port_id, "addr1") != 0) && (strcmp(port_id, "addr2") != 0) && (strcmp(port_id, "data1") != 0) && (strcmp(port_id, "data2") != 0)) {
                is_ram = false;
                break;
            }
        }
        /* port connections were passed by name; verify output port names */
        for (int i = 0; i < node->num_output_pins && is_ram; i++) {
            oassert(node->output_pins[i]->mapping);
            char* port_id = node->output_pins[i]->mapping;

            /* comparision of mapping ports with dual_port_ram port names */
            if ((strcmp(port_id, "out1") != 0) && (strcmp(port_id, "out2") != 0)) {
                is_ram = false;
                break;
            }
        }
    }

    return (is_ram);
}

bool is_ast_sp_ram(ast_node_t* node) {
    bool is_ram;
    ast_node_t* instance = node->children[0];
    is_ram = (!strcmp(node->identifier_node->types.identifier, SINGLE_PORT_RAM_string))
             && (instance->children[0]->num_children == 5);

    ast_node_t* connect_list = instance->children[0];
    if (is_ram && connect_list->children[0]->identifier_node) {
        /* port connections were passed by name; verify port names */
        for (int i = 0; i < connect_list->num_children && is_ram; i++) {
            char* id = connect_list->children[i]->identifier_node->types.identifier;

            if ((strcmp(id, "we") != 0) && (strcmp(id, "clk") != 0) && (strcmp(id, "addr") != 0) && (strcmp(id, "data") != 0) && (strcmp(id, "out") != 0)) {
                is_ram = false;
                break;
            }
        }
    }

    return is_ram;
}

bool is_ast_dp_ram(ast_node_t* node) {
    bool is_ram;
    ast_node_t* instance = node->children[0];
    is_ram = (!strcmp(node->identifier_node->types.identifier, DUAL_PORT_RAM_string))
             && (instance->children[0]->num_children == 9);

    ast_node_t* connect_list = instance->children[0];
    if (is_ram && connect_list->children[0]->identifier_node) {
        /* port connections were passed by name; verify port names */
        for (int i = 0; i < connect_list->num_children && is_ram; i++) {
            char* id = connect_list->children[i]->identifier_node->types.identifier;

            if ((strcmp(id, "clk") != 0) && (strcmp(id, "we1") != 0) && (strcmp(id, "we2") != 0) && (strcmp(id, "addr1") != 0) && (strcmp(id, "addr2") != 0) && (strcmp(id, "data1") != 0) && (strcmp(id, "data2") != 0) && (strcmp(id, "out1") != 0) && (strcmp(id, "out2") != 0)) {
                is_ram = false;
                break;
            }
        }
    }

    return is_ram;
}

sp_ram_signals* get_sp_ram_signals(nnode_t* node) {
    oassert(is_sp_ram(node));

    ast_node_t* ast_node = node->related_ast_node;
    sp_ram_signals* signals = (sp_ram_signals*)vtr::malloc(sizeof(sp_ram_signals));

    // Separate the input signals according to their mapping.
    signals->addr = init_signal_list();
    signals->data = init_signal_list();
    signals->out = init_signal_list();
    signals->we = NULL;
    signals->clk = NULL;

    int i;
    for (i = 0; i < node->num_input_pins; i++) {
        npin_t* pin = node->input_pins[i];
        if (!strcmp(pin->mapping, "addr"))
            add_pin_to_signal_list(signals->addr, pin);
        else if (!strcmp(pin->mapping, "data"))
            add_pin_to_signal_list(signals->data, pin);
        else if (!strcmp(pin->mapping, "we"))
            signals->we = pin;
        else if (!strcmp(pin->mapping, "clk"))
            signals->clk = pin;
        else
            error_message(NETLIST, ast_node->loc,
                          "Unexpected input pin mapping \"%s\" on memory node: %s\n",
                          pin->mapping, node->name);
    }

    oassert(signals->clk != NULL);
    oassert(signals->we != NULL);
    oassert(signals->addr->count >= 1);
    oassert(signals->data->count >= 1);
    oassert(signals->data->count == node->num_output_pins);

    for (i = 0; i < node->num_output_pins; i++) {
        npin_t* pin = node->output_pins[i];
        if (!strcmp(pin->mapping, "out"))
            add_pin_to_signal_list(signals->out, pin);
        else
            error_message(NETLIST, ast_node->loc,
                          "Unexpected output pin mapping \"%s\" on memory node: %s\n",
                          pin->mapping, node->name);
    }

    oassert(signals->out->count == signals->data->count);

    return signals;
}

void free_sp_ram_signals(sp_ram_signals* signalsvar) {
    free_signal_list(signalsvar->data);
    free_signal_list(signalsvar->addr);
    free_signal_list(signalsvar->out);

    vtr::free(signalsvar);
}

dp_ram_signals* get_dp_ram_signals(nnode_t* node) {
    oassert(is_dp_ram(node));

    ast_node_t* ast_node = node->related_ast_node;
    dp_ram_signals* signals = (dp_ram_signals*)vtr::malloc(sizeof(dp_ram_signals));

    // Separate the input signals according to their mapping.
    signals->addr1 = init_signal_list();
    signals->addr2 = init_signal_list();
    signals->data1 = init_signal_list();
    signals->data2 = init_signal_list();
    signals->out1 = init_signal_list();
    signals->out2 = init_signal_list();
    signals->we1 = NULL;
    signals->we2 = NULL;
    signals->clk = NULL;

    int i;
    for (i = 0; i < node->num_input_pins; i++) {
        npin_t* pin = node->input_pins[i];
        if (!strcmp(pin->mapping, "addr1"))
            add_pin_to_signal_list(signals->addr1, pin);
        else if (!strcmp(pin->mapping, "addr2"))
            add_pin_to_signal_list(signals->addr2, pin);
        else if (!strcmp(pin->mapping, "data1"))
            add_pin_to_signal_list(signals->data1, pin);
        else if (!strcmp(pin->mapping, "data2"))
            add_pin_to_signal_list(signals->data2, pin);
        else if (!strcmp(pin->mapping, "we1"))
            signals->we1 = pin;
        else if (!strcmp(pin->mapping, "we2"))
            signals->we2 = pin;
        else if (!strcmp(pin->mapping, "clk"))
            signals->clk = pin;
        else
            error_message(NETLIST, ast_node->loc,
                          "Unexpected input pin mapping \"%s\" on memory node: %s\n",
                          pin->mapping, node->name);
    }

    // Sanity checks.
    oassert(signals->clk != NULL);
    oassert(signals->we1 != NULL && signals->we2 != NULL);
    oassert(signals->addr1->count >= 1 && signals->data1->count >= 1);
    oassert(signals->addr2->count >= 1 && signals->data2->count >= 1);
    oassert(signals->addr1->count == signals->addr2->count);
    oassert(signals->data1->count == signals->data2->count);
    oassert(signals->data1->count + signals->data2->count == node->num_output_pins);

    // Separate output signals according to mapping.
    for (i = 0; i < node->num_output_pins; i++) {
        npin_t* pin = node->output_pins[i];
        if (!strcmp(pin->mapping, "out1"))
            add_pin_to_signal_list(signals->out1, pin);
        else if (!strcmp(pin->mapping, "out2"))
            add_pin_to_signal_list(signals->out2, pin);
        else
            error_message(NETLIST, ast_node->loc,
                          "Unexpected output pin mapping \"%s\" on memory node: %s\n",
                          pin->mapping, node->name);
    }

    oassert(signals->out1->count == signals->out2->count);
    oassert(signals->out1->count == signals->data1->count);

    return signals;
}

void free_dp_ram_signals(dp_ram_signals* signalsvar) {
    free_signal_list(signalsvar->data1);
    free_signal_list(signalsvar->data2);
    free_signal_list(signalsvar->addr1);
    free_signal_list(signalsvar->addr2);
    free_signal_list(signalsvar->out1);
    free_signal_list(signalsvar->out2);

    vtr::free(signalsvar);
}

/*
 * Expands the given single port ram block into soft logic.
 */
void instantiate_soft_single_port_ram(nnode_t* node, short mark, netlist_t* netlist) {
    oassert(is_sp_ram(node));

    sp_ram_signals* signals = get_sp_ram_signals(node);

    // Construct an address decoder.
    signal_list_t* decoder = create_decoder(node, mark, signals->addr);

    // The total number of memory addresses. (2^address_bits)
    long num_addr = decoder->count;

    nnode_t** and_gates = (nnode_t**)vtr::malloc(sizeof(nnode_t*) * num_addr);

    for (long i = 0; i < num_addr; i++) {
        npin_t* address_pin = decoder->pins[i];
        /* Check that the input pin is driven */
        oassert(
            address_pin->net->num_driver_pins
            || address_pin->net == syn_netlist->zero_net
            || address_pin->net == syn_netlist->one_net
            || address_pin->net == syn_netlist->pad_net);

        // An AND gate to enable and disable writing.
        nnode_t* and_g = make_1port_logic_gate(LOGICAL_AND, 2, node, mark);
        add_input_pin_to_node(and_g, address_pin, 0);

        if (!i)
            remap_pin_to_new_node(signals->we, and_g, 1);
        else
            add_input_pin_to_node(and_g, copy_input_npin(signals->we), 1);

        and_gates[i] = and_g;
    }

    for (long i = 0; i < signals->data->count; i++) {
        npin_t* data_pin = signals->data->pins[i];

        // The output multiplexer determines which memory cell is connected to the output register.
        nnode_t* output_mux = make_2port_gate(MULTI_PORT_MUX, num_addr, num_addr, 1, node, mark);

        int j;
        for (j = 0; j < num_addr; j++) {
            npin_t* address_pin = decoder->pins[j];
            /* Check that the input pin is driven */
            oassert(
                address_pin->net->num_driver_pins
                || address_pin->net == syn_netlist->zero_net
                || address_pin->net == syn_netlist->one_net
                || address_pin->net == syn_netlist->pad_net);

            // A multiplexer switches between accepting incoming data and keeping existing data.
            nnode_t* mux = make_2port_gate(MUX_2, 2, 2, 1, node, mark);
            nnode_t* not_g = make_not_gate(node, mark);
            connect_nodes(and_gates[j], 0, not_g, 0);
            connect_nodes(and_gates[j], 0, mux, 0);
            connect_nodes(not_g, 0, mux, 1);
            if (!j)
                remap_pin_to_new_node(data_pin, mux, 2);
            else
                add_input_pin_to_node(mux, copy_input_npin(data_pin), 2);

            // A flipflop holds the value of each memory cell.
            nnode_t* ff = make_2port_gate(FF_NODE, 1, 1, 1, node, mark);
            connect_nodes(mux, 0, ff, 0);
            if (!i && !j)
                remap_pin_to_new_node(signals->clk, ff, 1);
            else
                add_input_pin_to_node(ff, copy_input_npin(signals->clk), 1);

            // The output of the flipflop connects back to the multiplexer (to hold the value.)
            connect_nodes(ff, 0, mux, 3);

            // The flipflop connects to the output multiplexer.
            connect_nodes(ff, 0, output_mux, num_addr + j);

            // Hook the address pin up to the output mux.
            add_input_pin_to_node(output_mux, copy_input_npin(address_pin), j);
            ff->attributes->clk_edge_type = RISING_EDGE_SENSITIVITY;
        }

        npin_t* output_pin = node->output_pins[i];

        // Make sure the BLIF name comes directly from the MUX.
        if (output_pin->name)
            vtr::free(output_pin->name);
        output_pin->name = NULL;

        remap_pin_to_new_node(output_pin, output_mux, 0);
        instantiate_multi_port_mux(output_mux, mark, netlist);
    }

    vtr::free(and_gates);

    // Free signal lists.
    free_sp_ram_signals(signals);
    free_signal_list(decoder);

    // Free the original hard block memory.
    free_nnode(node);
}

/*
 * Expands the given dual port ram block into soft logic.
 */
void instantiate_soft_dual_port_ram(nnode_t* node, short mark, netlist_t* netlist) {
    oassert(is_dp_ram(node));

    dp_ram_signals* signals = get_dp_ram_signals(node);

    // Construct the address decoders.
    signal_list_t* decoder1 = create_decoder(node, mark, signals->addr1);
    signal_list_t* decoder2 = create_decoder(node, mark, signals->addr2);

    oassert(decoder1->count == decoder2->count);

    // The total number of memory addresses. (2^address_bits)
    int num_addr = decoder1->count;
    int data_width = signals->data1->count;

    // Arrays of common gates, one per address.
    nnode_t** and1_gates = (nnode_t**)vtr::malloc(sizeof(nnode_t*) * num_addr);
    nnode_t** and2_gates = (nnode_t**)vtr::malloc(sizeof(nnode_t*) * num_addr);
    nnode_t** or_gates = (nnode_t**)vtr::malloc(sizeof(nnode_t*) * num_addr);

    int i;
    for (i = 0; i < num_addr; i++) {
        npin_t* addr1_pin = decoder1->pins[i];
        npin_t* addr2_pin = decoder2->pins[i];

        oassert(
            addr1_pin->net->num_driver_pins
            || addr1_pin->net == syn_netlist->zero_net
            || addr1_pin->net == syn_netlist->one_net
            || addr1_pin->net == syn_netlist->pad_net);
        oassert(
            addr2_pin->net->num_driver_pins
            || addr2_pin->net == syn_netlist->zero_net
            || addr2_pin->net == syn_netlist->one_net
            || addr2_pin->net == syn_netlist->pad_net);

        // Write enable and gate for address 1.
        nnode_t* and1 = make_1port_logic_gate(LOGICAL_AND, 2, node, mark);
        add_input_pin_to_node(and1, addr1_pin, 0);

        if (!i)
            remap_pin_to_new_node(signals->we1, and1, 1);
        else
            add_input_pin_to_node(and1, copy_input_npin(signals->we1), 1);

        // Write enable and gate for address 2.
        nnode_t* and2 = make_1port_logic_gate(LOGICAL_AND, 2, node, mark);
        add_input_pin_to_node(and2, addr2_pin, 0);

        if (!i)
            remap_pin_to_new_node(signals->we2, and2, 1);
        else
            add_input_pin_to_node(and2, copy_input_npin(signals->we2), 1);

        and1_gates[i] = and1;
        and2_gates[i] = and2;

        // OR, to enable writing to this address when either port selects it for writing.
        nnode_t* or_g = make_1port_logic_gate(LOGICAL_OR, 2, node, mark);
        connect_nodes(and1, 0, or_g, 0);
        connect_nodes(and2, 0, or_g, 1);

        or_gates[i] = or_g;
    }

    for (i = 0; i < data_width; i++) {
        npin_t* data1_pin = signals->data1->pins[i];
        npin_t* data2_pin = signals->data2->pins[i];

        // The output multiplexer determines which memory cell is connected to the output register.
        nnode_t* output_mux1 = make_2port_gate(MULTI_PORT_MUX, num_addr, num_addr, 1, node, mark);
        nnode_t* output_mux2 = make_2port_gate(MULTI_PORT_MUX, num_addr, num_addr, 1, node, mark);

        int j;
        for (j = 0; j < num_addr; j++) {
            npin_t* addr1_pin = decoder1->pins[j];
            npin_t* addr2_pin = decoder2->pins[j];

            oassert(
                addr1_pin->net->num_driver_pins
                || addr1_pin->net == syn_netlist->zero_net
                || addr1_pin->net == syn_netlist->one_net
                || addr1_pin->net == syn_netlist->pad_net);
            oassert(
                addr2_pin->net->num_driver_pins
                || addr2_pin->net == syn_netlist->zero_net
                || addr2_pin->net == syn_netlist->one_net
                || addr2_pin->net == syn_netlist->pad_net);

            // The data mux selects between the two data lines for this address.
            nnode_t* data_mux = make_2port_gate(MUX_2, 2, 2, 1, node, mark);
            // Port 2 before 1 to mimic the simulator's behaviour when the addresses are the same.
            connect_nodes(and2_gates[j], 0, data_mux, 0);
            connect_nodes(and1_gates[j], 0, data_mux, 1);
            if (!j)
                remap_pin_to_new_node(data2_pin, data_mux, 2);
            else
                add_input_pin_to_node(data_mux, copy_input_npin(data2_pin), 2);
            if (!j)
                remap_pin_to_new_node(data1_pin, data_mux, 3);
            else
                add_input_pin_to_node(data_mux, copy_input_npin(data1_pin), 3);

            nnode_t* not_g = make_not_gate(node, mark);
            connect_nodes(or_gates[j], 0, not_g, 0);

            // A multiplexer switches between accepting incoming data and keeping existing data.
            nnode_t* mux = make_2port_gate(MUX_2, 2, 2, 1, node, mark);
            connect_nodes(or_gates[j], 0, mux, 0);
            connect_nodes(not_g, 0, mux, 1);
            connect_nodes(data_mux, 0, mux, 2);

            // A flipflop holds the value of each memory cell.
            nnode_t* ff = make_2port_gate(FF_NODE, 1, 1, 1, node, mark);
            connect_nodes(mux, 0, ff, 0);
            if (!i && !j)
                remap_pin_to_new_node(signals->clk, ff, 1);
            else
                add_input_pin_to_node(ff, copy_input_npin(signals->clk), 1);

            // The output of the flipflop connects back to the multiplexer (to hold the value.)
            connect_nodes(ff, 0, mux, 3);

            // Connect the flipflop to both output muxes.
            connect_nodes(ff, 0, output_mux1, num_addr + j);
            connect_nodes(ff, 0, output_mux2, num_addr + j);

            // Connect address lines to the output muxes for this address.
            add_input_pin_to_node(output_mux1, copy_input_npin(addr1_pin), j);
            add_input_pin_to_node(output_mux2, copy_input_npin(addr2_pin), j);
            ff->attributes->clk_edge_type = RISING_EDGE_SENSITIVITY;
        }

        npin_t* out1_pin = signals->out1->pins[i];
        npin_t* out2_pin = signals->out2->pins[i];

        // Make sure the BLIF name comes directly from the MUX.
        if (out1_pin->name)
            vtr::free(out1_pin->name);
        out1_pin->name = NULL;

        if (out2_pin->name)
            vtr::free(out2_pin->name);
        out2_pin->name = NULL;

        remap_pin_to_new_node(out1_pin, output_mux1, 0);
        remap_pin_to_new_node(out2_pin, output_mux2, 0);

        // Convert the output muxes to MUX_2 nodes.
        instantiate_multi_port_mux(output_mux1, mark, netlist);
        instantiate_multi_port_mux(output_mux2, mark, netlist);
    }

    vtr::free(and1_gates);
    vtr::free(and2_gates);
    vtr::free(or_gates);

    // Free signal lists.
    free_dp_ram_signals(signals);
    free_signal_list(decoder1);
    free_signal_list(decoder2);

    // Free the original hard block memory.
    free_nnode(node);
}

/*
 * Creates an n to 2^n decoder from the input signal list.
 */
signal_list_t* create_decoder(nnode_t* node, short mark, signal_list_t* input_list) {
    long num_inputs = input_list->count;
    if (num_inputs > SOFT_RAM_ADDR_LIMIT)
        error_message(NETLIST, node->loc, "Memory %s of depth 2^%ld exceeds ODIN bound of 2^%d.\nMust use an FPGA architecture that contains embedded hard block memories", node->name, num_inputs, SOFT_RAM_ADDR_LIMIT);

    // Number of outputs is 2^num_inputs
    long num_outputs = shift_left_value_with_overflow_check(0x1, num_inputs, node->loc);

    // Create NOT gates for all inputs and put the outputs in their own signal list.
    signal_list_t* not_gates = init_signal_list();
    for (long i = 0; i < num_inputs; i++) {
        if (!input_list->pins[i]->net->num_driver_pins
            && input_list->pins[i]->net != syn_netlist->zero_net
            && input_list->pins[i]->net != syn_netlist->one_net
            && input_list->pins[i]->net != syn_netlist->pad_net) {
            warning_message(NETLIST, node->loc, "Signal %s is not driven. padding with ground\n", input_list->pins[i]->name);
            add_fanout_pin_to_net(syn_netlist->zero_net, input_list->pins[i]);
        }

        nnode_t* not_g = make_not_gate(node, mark);
        remap_pin_to_new_node(input_list->pins[i], not_g, 0);
        npin_t* not_output = allocate_npin();
        add_output_pin_to_node(not_g, not_output, 0);
        nnet_t* net = allocate_nnet();
        net->name = make_full_ref_name(NULL, NULL, NULL, not_g->name, 0);
        add_driver_pin_to_net(net, not_output);
        not_output = allocate_npin();
        add_fanout_pin_to_net(net, not_output);
        add_pin_to_signal_list(not_gates, not_output);

        npin_t* pin = allocate_npin();
        net = input_list->pins[i]->net;

        add_fanout_pin_to_net(net, pin);

        input_list->pins[i] = pin;
    }

    // Create AND gates and assign signals.
    signal_list_t* return_list = init_signal_list();
    for (long i = 0; i < num_outputs; i++) {
        // Each output is connected to an and gate which is driven by a single permutation of the inputs.
        nnode_t* and_g = make_1port_logic_gate(LOGICAL_AND, num_inputs, node, mark);

        for (long j = 0; j < num_inputs; j++) {
            // Look at the jth bit of i. If it's 0, take the negated signal.
            long value = shift_left_value_with_overflow_check(0x1, j, and_g->loc);
            value &= i;
            value >>= j;

            npin_t* pin = value ? input_list->pins[j] : not_gates->pins[j];

            // Use the original not pins on the first iteration and the original input pins on the last.
            if (i > 0 && i < num_outputs - 1)
                pin = copy_input_npin(pin);

            // Connect the signal to the output and gate.
            add_input_pin_to_node(and_g, pin, j);
        }

        // Add output pin, net, and fanout pin.
        npin_t* output = allocate_npin();
        nnet_t* net = allocate_nnet();
        add_output_pin_to_node(and_g, output, 0);
        net->name = make_full_ref_name(NULL, NULL, NULL, and_g->name, 0);
        add_driver_pin_to_net(net, output);
        output = allocate_npin();
        add_fanout_pin_to_net(net, output);

        // Add the fanout pin (decoder output) to the return list.
        add_pin_to_signal_list(return_list, output);
    }

    free_signal_list(not_gates);
    return return_list;
}

/**
 * (function: create_single_port_rom)
 * 
 * @brief create a single port ram with the given spram signals
 *
 * @param signals spram signals
 * @param node corresponding netlist node
 * 
 * @return a new single port ram
 */
nnode_t* create_single_port_ram(sp_ram_signals* spram_signals, nnode_t* node) {
    /* sanity checks */
    oassert(spram_signals->clk != NULL);
    oassert(spram_signals->we != NULL);
    oassert(spram_signals->addr->count >= 1);
    oassert(spram_signals->data->count >= 1);
    if (spram_signals->out != NULL) {
        oassert(spram_signals->data->count == spram_signals->out->count);
    }

    /* create a single port ram node */
    nnode_t* spram = allocate_nnode(node->loc);

    spram->type = MEMORY;
    /* some information from ast node is needed in partial mapping */
    char* hb_name = vtr::strdup(SINGLE_PORT_RAM_string);
    spram->name = node_name(spram, hb_name);
    spram->attributes->memory_id = vtr::strdup(node->attributes->memory_id);

    /* Create a fake ast node. */
    spram->related_ast_node = create_node_w_type(RAM, node->loc);
    spram->related_ast_node->identifier_node = create_tree_node_id(hb_name, node->loc);

    /* INPUTS */
    /* hook address portd into spram */
    add_input_port_to_memory(spram, spram_signals->addr, "addr");

    /* hook data ports into spram */
    add_input_port_to_memory(spram, spram_signals->data, "data");

    /* hook enable pins to spram */
    signal_list_t* we = init_signal_list();
    add_pin_to_signal_list(we, spram_signals->we);
    add_input_port_to_memory(spram, we, "we");

    /* hook clk pin into spram */
    signal_list_t* clk = init_signal_list();
    add_pin_to_signal_list(clk, spram_signals->clk);
    add_input_port_to_memory(spram, clk, "clk");

    /* OUTPUT */
    if (spram_signals->out == NULL) {
        /* init the signal list */
        spram_signals->out = init_signal_list();
        for (int i = 0; i < spram_signals->data->count; i++) {
            npin_t* new_pin = allocate_npin();
            nnet_t* new_net = allocate_nnet();
            /* add pin as the net driver */
            add_driver_pin_to_net(new_net, new_pin);
            /* store them into dpram signals */
            add_pin_to_signal_list(spram_signals->out, new_pin);
        }
    }
    add_output_port_to_memory(spram, spram_signals->out, "out");

    /* already compatible with SPRAM config so we leave it as is */
    sp_memory_list = insert_in_vptr_list(sp_memory_list, spram);
    /* register the SPRAM in arch model to have the related model in BLIF for simulation */
    register_memory_model(spram);

    // CLEAN UP
    free_signal_list(we);
    free_signal_list(clk);

    return (spram);
}

/**
 * (function: create_dual_port_rom)
 * 
 * @brief create a dual port ram with the given dpram signals
 * 
 * @param signals dpram signals
 * @param node corresponding netlist node
 * 
 * @return a new dual port ram
 */
nnode_t* create_dual_port_ram(dp_ram_signals* dpram_signals, nnode_t* node) {
    /* sanity checks */
    oassert(dpram_signals->clk);
    oassert((dpram_signals->we1) && (dpram_signals->we2));
    oassert((dpram_signals->addr1) && (dpram_signals->addr2));
    oassert((dpram_signals->data1) && (dpram_signals->data1));
    oassert(dpram_signals->addr1->count == dpram_signals->addr2->count);
    oassert(dpram_signals->data1->count == dpram_signals->data2->count);
    oassert(dpram_signals->addr1->count >= 1 && dpram_signals->data1->count >= 1);
    oassert(dpram_signals->addr2->count >= 1 && dpram_signals->data2->count >= 1);

    if (dpram_signals->out1 != NULL) {
        oassert(dpram_signals->data1->count == dpram_signals->out1->count);
    }
    if (dpram_signals->out2 != NULL) {
        oassert(dpram_signals->data2->count == dpram_signals->out2->count);
    }

    /* create a dual port ram node */
    nnode_t* dpram = allocate_nnode(node->loc);

    dpram->type = MEMORY;
    /* some information from ast node is needed in partial mapping */
    char* hb_name = vtr::strdup(DUAL_PORT_RAM_string);
    dpram->name = node_name(dpram, hb_name);
    dpram->attributes->memory_id = vtr::strdup(node->attributes->memory_id);

    /* Create a fake ast node. */
    dpram->related_ast_node = create_node_w_type(RAM, node->loc);
    dpram->related_ast_node->identifier_node = create_tree_node_id(hb_name, node->loc);

    /* INPUTS */
    /* hook address portd into dpram */
    add_input_port_to_memory(dpram, dpram_signals->addr1, "addr1");
    add_input_port_to_memory(dpram, dpram_signals->addr2, "addr2");

    /* hook data ports into dpram */
    add_input_port_to_memory(dpram, dpram_signals->data1, "data1");
    add_input_port_to_memory(dpram, dpram_signals->data2, "data2");

    /* hook enable pins to dpram */
    signal_list_t* we1 = init_signal_list();
    add_pin_to_signal_list(we1, dpram_signals->we1);
    add_input_port_to_memory(dpram, we1, "we1");

    signal_list_t* we2 = init_signal_list();
    add_pin_to_signal_list(we2, dpram_signals->we2);
    add_input_port_to_memory(dpram, we2, "we2");

    /* hook clk pin into dpram */
    signal_list_t* clk = init_signal_list();
    add_pin_to_signal_list(clk, dpram_signals->clk);
    add_input_port_to_memory(dpram, clk, "clk");

    /* OUTPUT */
    if (dpram_signals->out1 == NULL) {
        /* init the signal list */
        dpram_signals->out1 = init_signal_list();
        for (int i = 0; i < dpram_signals->data1->count; i++) {
            npin_t* new_pin = allocate_npin();
            nnet_t* new_net = allocate_nnet();
            /* add pin as the net driver */
            add_driver_pin_to_net(new_net, new_pin);
            /* store them into dpram signals */
            add_pin_to_signal_list(dpram_signals->out1, new_pin);
        }
    }
    add_output_port_to_memory(dpram, dpram_signals->out1, "out1");
    if (dpram_signals->out2 == NULL) {
        /* init the signal list */
        dpram_signals->out2 = init_signal_list();
        for (int i = 0; i < dpram_signals->data2->count; i++) {
            npin_t* new_pin = allocate_npin();
            nnet_t* new_net = allocate_nnet();
            /* add pin as the net driver */
            add_driver_pin_to_net(new_net, new_pin);
            /* store them into dpram signals */
            add_pin_to_signal_list(dpram_signals->out2, new_pin);
        }
    }
    add_output_port_to_memory(dpram, dpram_signals->out2, "out2");

    /* already compatible with DPRAM config so we leave it as is */
    dp_memory_list = insert_in_vptr_list(dp_memory_list, dpram);
    /* register the DPRAM in arch model to have the related model in BLIF for simulation */
    register_memory_model(dpram);

    // CLEAN UP
    free_signal_list(we1);
    free_signal_list(we2);
    free_signal_list(clk);

    return (dpram);
}

/**
 * (function: register_memory_model)
 * 
 * @brief register the corresponding memory hard if any is 
 * available in the given architecture
 * 
 * @param mem pointing to the memory node
 */
void register_memory_model(nnode_t* mem) {
    /* See if the hard block declared is supported by FPGA architecture */
    t_model* hb_model = find_hard_block(mem->related_ast_node->identifier_node->types.identifier);

    if (hb_model) {
        /* Declare the hard block as used for the blif generation */
        hb_model->used = 1;
    }
}

/**
 * (function: resolve_single_port_ram)
 * 
 * @brief resolve the spram block by reordering the input signals 
 * to be compatible with Odin's partial mapping phase
 * 
 * @param node pointing to a spram node
 * @param traverse_mark_number unique traversal mark for blif elaboration pass
 * @param netlist pointer to the current netlist file
 */
void resolve_single_port_ram(nnode_t* node, uintptr_t traverse_mark_number, netlist_t* /* netlist */) {
    oassert(node->traverse_visited == traverse_mark_number);
    oassert(node->num_input_port_sizes == 4);
    oassert(node->num_output_port_sizes == 1);

    /* check if the node is a valid spram */
    if (!is_blif_sp_ram(node))
        error_message(BLIF_ELABORATION, node->loc,
                      "SPRAM (%s) ports mismatch with VTR single_port_ram hard block ports\n", node->name);

    /** 
     * blif single port ram information 
     * 
     * ADDR:    input port [0] 
     * CLOCK:   input port [1] 
     * DATAIN:  input port [2] 
     * WE:      input port [3] 
     * 
     * DATAOUT: output port [0] 
     */

    int i;
    int SP_ADDR_width = node->input_port_sizes[0];
    int SP_CLK_width = node->input_port_sizes[1]; // should be 1
    int SP_DATA_width = node->input_port_sizes[2];
    int SP_WE_width = node->input_port_sizes[3]; // should be 1
    int SP_OUT_width = node->output_port_sizes[0];

    /* validate the data width */
    oassert(SP_CLK_width == 1 && SP_WE_width == 1);
    oassert(SP_DATA_width == node->output_port_sizes[0]);

    /* creating dpram node for the range of data width */
    int offset = 0;
    /* creating a new node */
    sp_ram_signals* signals = init_sp_ram_signals();

    /* INPUTS */
    /* adding the addr signals */
    for (i = 0; i < SP_ADDR_width; i++) {
        npin_t* pin = node->input_pins[offset + i];
        /* detach from the main node, since it will be connected to a new dpram */
        pin->node->input_pins[pin->pin_node_idx] = NULL;

        add_pin_to_signal_list(signals->addr, pin);
    }
    offset += SP_ADDR_width;

    /* adding the clk signals */
    npin_t* clk_pin = node->input_pins[offset];
    /* detach from the main node, since it will be connected to a new dpram */
    clk_pin->node->input_pins[clk_pin->pin_node_idx] = NULL;
    signals->clk = clk_pin;
    /* increment offset */
    offset += 1;

    /* adding the data signals */
    for (i = 0; i < SP_DATA_width; i++) {
        /* hook the data1 pin to new node */
        npin_t* pin = node->input_pins[offset + i];
        /* in case of padding, pins have not been remapped, need to detach them from the BRAM node */
        pin->node->input_pins[pin->pin_node_idx] = NULL;

        add_pin_to_signal_list(signals->data, pin);
    }
    offset += SP_DATA_width;

    /* adding the we signals */
    npin_t* we_pin = node->input_pins[offset];
    /* detach from the main node, since it will be connected to a new dpram */
    we_pin->node->input_pins[we_pin->pin_node_idx] = NULL;
    signals->we = we_pin;

    /* OUTPUT */
    /* adding the output signals */
    offset = 0;
    for (i = 0; i < SP_OUT_width; i++) {
        /* hook the data1 pin to new node */
        npin_t* pin = node->output_pins[offset + i];
        /* in case of padding, pins have not been remapped, need to detach them from the BRAM node */
        pin->node->output_pins[pin->pin_node_idx] = NULL;

        add_pin_to_signal_list(signals->out, pin);
    }

    /* creating a new spram with size modified input signals */
    create_single_port_ram(signals, node);

    // CLEAN UP
    free_nnode(node);
    free_sp_ram_signals(signals);
}

/**
 * (function: resolve_dual_port_ram)
 * 
 * @brief resolve the dpram block by reordering the input signals 
 * to be compatible with Odin's partial mapping phase
 * 
 * @param node pointing to a dual port ram node
 * @param traverse_mark_number unique traversal mark for blif elaboration pass
 * @param netlist pointer to the current netlist file
 */
void resolve_dual_port_ram(nnode_t* node, uintptr_t traverse_mark_number, netlist_t* netlist) {
    oassert(node->traverse_visited == traverse_mark_number);
    oassert(node->num_input_port_sizes == 7);
    oassert(node->num_output_port_sizes == 2);

    /* check if the node is a valid spram */
    if (!is_blif_dp_ram(node))
        error_message(BLIF_ELABORATION, node->loc,
                      "DPRAM (%s) ports mismatch with VTR dual_port_ram hard block ports\n", node->name);

    /** 
     * blif dual port ram information 
     * 
     * ADDR1:    input port [0] 
     * ADDR2:    input port [1] 
     * CLOCK:    input port [2] 
     * DATAIN1:  input port [3] 
     * DATAIN2:  input port [4] 
     * WE1:      input port [5] 
     * WE2:      input port [6] 
     * 
     * DATAOUT1: output port [0] 
     * DATAOUT2: output port [1] 
     */
    int i;
    int DP_ADDR1_width = node->input_port_sizes[0];
    int DP_ADDR2_width = node->input_port_sizes[1];
    int DP_CLK_width = node->input_port_sizes[2]; // should be 1
    int DP_DATA1_width = node->input_port_sizes[3];
    int DP_DATA2_width = node->input_port_sizes[4];
    int DP_WE1_width = node->input_port_sizes[5]; // should be 1
    int DP_WE2_width = node->input_port_sizes[6]; // should be 1
    int DP_OUT1_width = node->output_port_sizes[0];
    int DP_OUT2_width = node->output_port_sizes[1];

    /* validate the port width */
    oassert((DP_CLK_width == 1) && (DP_WE1_width == 1) && (DP_WE2_width == 1));
    oassert(DP_DATA1_width == DP_DATA2_width);
    oassert(DP_DATA1_width == node->output_port_sizes[0]);
    oassert(DP_DATA1_width == node->output_port_sizes[1]);

    int max_addr_width = std::max(DP_ADDR1_width, DP_ADDR2_width);

    /* creating dpram node for the range of data width */
    int offset = 0;
    /* creating a new node */
    dp_ram_signals* signals = init_dp_ram_signals();

    /* INPUTS */
    /* adding the addr1 signals */
    for (i = 0; i < max_addr_width; i++) {
        /* hook the addr1 pin to new node */
        if (i < DP_ADDR1_width) {
            npin_t* pin = node->input_pins[offset + i];
            /* in case of padding, pins have not been remapped, need to detach them from the BRAM node */
            pin->node->input_pins[pin->pin_node_idx] = NULL;

            add_pin_to_signal_list(signals->addr1, pin);
        } else {
            add_pin_to_signal_list(signals->addr1, get_pad_pin(netlist));
        }
    }
    offset += DP_ADDR1_width;

    /* adding the addr2 signals */
    for (i = 0; i < max_addr_width; i++) {
        /* hook the addr1 pin to new node */
        if (i < DP_ADDR2_width) {
            npin_t* pin = node->input_pins[offset + i];
            /* in case of padding, pins have not been remapped, need to detach them from the BRAM node */
            pin->node->input_pins[pin->pin_node_idx] = NULL;

            add_pin_to_signal_list(signals->addr2, pin);
        } else {
            add_pin_to_signal_list(signals->addr2, get_pad_pin(netlist));
        }
    }
    offset += DP_ADDR2_width;

    /* adding the clk signals */
    npin_t* clk_pin = node->input_pins[offset];
    /* detach from the main node, since it will be connected to a new dpram */
    clk_pin->node->input_pins[clk_pin->pin_node_idx] = NULL;
    signals->clk = clk_pin;
    /* increment offset */
    offset += 1;

    /* adding the data1 signals */
    for (i = 0; i < DP_DATA1_width; i++) {
        /* hook the data1 pin to new node */
        npin_t* pin = node->input_pins[offset + i];
        /* in case of padding, pins have not been remapped, need to detach them from the BRAM node */
        pin->node->input_pins[pin->pin_node_idx] = NULL;

        add_pin_to_signal_list(signals->data1, pin);
    }
    offset += DP_DATA1_width;

    /* adding the data2 signals */
    for (i = 0; i < DP_DATA2_width; i++) {
        /* hook the data1 pin to new node */
        npin_t* pin = node->input_pins[offset + i];
        /* in case of padding, pins have not been remapped, need to detach them from the BRAM node */
        pin->node->input_pins[pin->pin_node_idx] = NULL;

        add_pin_to_signal_list(signals->data2, pin);
    }
    offset += DP_DATA2_width;

    /* adding the we1 signals */
    npin_t* we1_pin = node->input_pins[offset];
    /* detach from the main node, since it will be connected to a new dpram */
    we1_pin->node->input_pins[we1_pin->pin_node_idx] = NULL;
    signals->we1 = we1_pin;
    /* increment offset */
    offset += 1;

    /* adding the we2 signals */
    npin_t* we2_pin = node->input_pins[offset];
    /* detach from the main node, since it will be connected to a new dpram */
    we2_pin->node->input_pins[we2_pin->pin_node_idx] = NULL;
    signals->we2 = we2_pin;

    /* OUTPUT */
    offset = 0;
    /* adding the output1 signals */
    for (i = 0; i < DP_OUT1_width; i++) {
        /* hook the data1 pin to new node */
        npin_t* pin = node->output_pins[offset + i];
        /* in case of padding, pins have not been remapped, need to detach them from the BRAM node */
        pin->node->output_pins[pin->pin_node_idx] = NULL;

        add_pin_to_signal_list(signals->out1, pin);
    }
    offset += DP_OUT1_width;

    /* adding the output1 signals */
    for (i = 0; i < DP_OUT2_width; i++) {
        /* hook the data1 pin to new node */
        npin_t* pin = node->output_pins[offset + i];
        /* in case of padding, pins have not been remapped, need to detach them from the BRAM node */
        pin->node->output_pins[pin->pin_node_idx] = NULL;

        add_pin_to_signal_list(signals->out2, pin);
    }

    /* creating a new dpram with size modified input signals */
    create_dual_port_ram(signals, node);

    // CLEAN UP
    free_nnode(node);
    free_dp_ram_signals(signals);
}
