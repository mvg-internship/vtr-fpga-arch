# Libarchfpga build

See the [optional Build Information](https://github.com/verilog-to-routing/vtr-verilog-to-routing/blob/master/doc/src/vtr/optional_build_info.md) to get started with the tool. 

## How to start with fpgaviz
To get started with fpgaviz after building the project we need:

```
$ cd VTR_ROOT/build/libs/libarchfpga
$ ./fpgaviz [path_to_read] [path_to_save]
```
[path_to_read] - location of the XML-file to read.<br/>
[path_to_save] - the desired location of the XML-file to save.


## Samples for testing
XML-files for testing are located in:
```
$ cd VTR_ROOT/vtr_flow/arch
```
Be careful with choosing a sample! fpgaviz accepts only fixed-size architecture.
