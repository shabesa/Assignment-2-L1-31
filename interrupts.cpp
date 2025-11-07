/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 */

#include "interrupts.hpp"

std::tuple<std::string, std::string, int> simulate_trace(std::vector<std::string> trace_file, int time, std::vector<std::string> vectors, std::vector<int> delays, std::vector<external_file> external_files, PCB current, std::vector<PCB> wait_queue) {

    std::string trace;      //!< string to store single line of trace file
    std::string execution = "";  //!< string to accumulate the execution output
    std::string system_status = "";  //!< string to accumulate the system status output
    int current_time = time;

    //parse each line of the input trace file. 'for' loop to keep track of indices.
    for(size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];

        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if(activity == "CPU") { //As per Assignment 1
            execution += simulate_cpu(duration_intr, current_time);
        } else if(activity == "SYSCALL") { //As per Assignment 1
            execution += handle_interrupt(duration_intr, current_time, vectors, delays, "SYSCALL ISR");
        } else if(activity == "END_IO") {
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            current_time = time;
            execution += intr;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR(ADD STEPS HERE)\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "FORK") {
            auto [intr, time] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = time;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your FORK output here
            
            unsigned int child_pid = get_next_pid(wait_queue);
            int child_partition = find_available_partition(current.size, wait_queue);

            if(child_partition == -1) {
                execution += std::to_string(current_time) + ", FORK ERROR: No available partition\n";
            } else {
                PCB child(child_pid, current.PID, program_name, current.size, child_partition);
                
                execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", cloning the PCB\n";
                memory[child_partition - 1].code = current.program_name;
                current_time += duration_intr;

                execution += std::to_string(current_time) + ", 0, scheduler called\n";
                execution += std::to_string(current_time) + ", 1, IRET\n";
                current_time += 1;

                wait_queue.push_back(child);

                system_status += "time: " + std::to_string(current_time) + "; current trace: FORK, " 
                        + std::to_string(duration_intr) + "\n";
                system_status += "+------------------------------------------------------+\n";
                system_status += "| PID |program name |partition number | size |   state |\n";
                system_status += "+------------------------------------------------------+\n";

                // Display child PCB (running)
                system_status += "|   " + std::to_string(child_pid) + " |    " + current.program_name 
                        + " |               " + std::to_string(child_partition) + " |    " 
                        + std::to_string(current.size) + " | running |\n";

                system_status += "|   " + std::to_string(current.PID) + " |    " + current.program_name 
                                + " |               " + std::to_string(current.partition_number) + " |    " 
                                + std::to_string(current.size) + " | waiting |\n";

                // Display waiting queue PCBs
                for (const auto& program : wait_queue) {
                    if(program.PID != child_pid) {
                        system_status += "|   " + std::to_string(program.PID) + " |    " + program.program_name 
                                + " |               " + std::to_string(program.partition_number) + " |    " 
                                + std::to_string(program.size) + " | waiting |\n";
                    }
                }
                system_status += "+------------------------------------------------------+\n\n";
            }

            ///////////////////////////////////////////////////////////////////////////////////////////

            //The following loop helps you do 2 things:
            // * Collect the trace of the child (and only the child, skip parent)
            // * Get the index of where the parent is supposed to start executing from
            std::vector<std::string> child_trace;
            bool skip = true;
            bool exec_flag = false;
            int parent_index = 0;

            for(size_t j = i; j < trace_file.size(); j++) {
                auto [_activity, _duration, _pn] = parse_trace(trace_file[j]);
                if(skip && _activity == "IF_CHILD") {
                    skip = false;
                    continue;
                } else if(_activity == "IF_PARENT"){
                    skip = true;
                    parent_index = j;
                    if(exec_flag) {
                        break;
                    }
                } else if(skip && _activity == "ENDIF") {
                    skip = false;
                    continue;
                } else if(!skip && _activity == "EXEC") {
                    skip = true;
                    child_trace.push_back(trace_file[j]);
                    exec_flag = true;
                }

                if(!skip) {
                    child_trace.push_back(trace_file[j]);
                }
            }
            i = parent_index;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the child's trace, run the child (HINT: think recursion)
            
            if(child_partition != -1) {
                auto [child_execution, child_status, new_time] = simulate_trace(child_trace, current_time, vectors, delays, external_files, wait_queue.back(), wait_queue);
                execution += child_execution;
                system_status += child_status;
                current_time = new_time;

                memory[child_partition - 1].code = "empty";
            }

            ///////////////////////////////////////////////////////////////////////////////////////////

        } else if(activity == "EXEC") {
            auto [intr, time] = intr_boilerplate(current_time, 3, 10, vectors);
            current_time = time;
            execution += intr;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your EXEC output here
            
            int avail_exec_partition = -1;
            unsigned int exec_size = get_program_size(program_name, external_files);

            if (exec_size == 0) {
                execution += std::to_string(current_time) + ", EXEC ERROR: Program not found\n";
            } else {
                avail_exec_partition = find_available_partition(exec_size, wait_queue);

                if (avail_exec_partition == -1) {
                    execution += std::to_string(current_time) + ", EXEC ERROR: No available partition\n";
                } else {

                    PCB exec_pcb(current.PID, current.PPID, program_name, exec_size, avail_exec_partition);
                    
                    unsigned int parent_pid = current.PID;
                    int parent_partition = current.partition_number;
                    std::string parent_program_name = current.program_name;
                    unsigned int parent_size = current.size;

                    execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", Program is " 
                                                                    + std::to_string(exec_size) + " Mb large\n";
                    current_time += duration_intr;

                    execution += std::to_string(current_time) + ", " + std::to_string(exec_size * 15) + ", loading program into memory\n";
                    std::cout << "exec_size: " << exec_size << std::endl;
                    current_time += (exec_size * 15);

                    execution += std::to_string(current_time) + ", 3, marking partition as occupied\n";
                    current_time += 3;

                    execution += std::to_string(current_time) + ", 6, updating PCB\n";
                    current_time += 6;

                    memory[avail_exec_partition - 1].code = program_name;

                    execution += std::to_string(current_time) + ", 0, scheduler called\n";
                    execution += std::to_string(current_time) + ", 1, IRET\n";
                    current_time += 1;

                    ///////////////////////////////////////////////////////////////////////////////////////////

                    std::ifstream exec_trace_file(program_name + ".txt");

                    std::vector<std::string> exec_traces;
                    std::string exec_trace;
                    while(std::getline(exec_trace_file, exec_trace)) {
                        exec_traces.push_back(exec_trace);
                    }

                    ///////////////////////////////////////////////////////////////////////////////////////////
                    //With the exec's trace (i.e. trace of external program), run the exec (HINT: think recursion)

                    if(exec_size != 0 && avail_exec_partition != -1) {
                        PCB exec_pcb(current.PID, current.PPID, program_name, exec_size, avail_exec_partition);
                        
                        auto [exec_execution, exec_status, exec_time] = simulate_trace(exec_traces, current_time, vectors, delays, external_files, exec_pcb, wait_queue);
                        execution += exec_execution;
                        system_status += exec_status;
                        current_time = exec_time;
                        
                        memory[avail_exec_partition - 1].code = "empty";
                    }

                    // Output status AFTER recursion (show running program + parent if exists)
                    system_status += "time: " + std::to_string(current_time) + "; current trace: EXEC, " + std::to_string(duration_intr) + "\n";
                    system_status += "+------------------------------------------------------+\n";
                    system_status += "| PID |program name |partition number | size |   state |\n";
                    system_status += "+------------------------------------------------------+\n";

                    // Show running program
                    system_status += "|   " + std::to_string(exec_pcb.PID) + " |    " + program_name + " |               " 
                            + std::to_string(avail_exec_partition) + " |    " + std::to_string(exec_size) + " | running |\n";

                    // Show parent if exists (not in wait_queue, but we know it's there from PPID)
                    if(current.PPID != -1) {
                        system_status += "|   " + std::to_string(current.PPID) + " |    " + "init" 
                                + " |               " + std::to_string(6) + " |    " 
                                + std::to_string(1) + " | waiting |\n";
                    }

                    system_status += "+------------------------------------------------------+\n\n";
                    system_status += "+------------------------------------------------------+\n\n";
                }
            }

            break; //Why is this important? (answer in report)
        }
    }

    return {execution, system_status, current_time};
}

int main(int argc, char** argv) {

    //vectors is a C++ std::vector of strings that contain the address of the ISR
    //delays  is a C++ std::vector of ints that contain the delays of each device
    //the index of these elements is the device number, starting from 0
    //external_files is a C++ std::vector of the struct 'external_file'. Check the struct in 
    //interrupt.hpp to know more.
    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    //Just a sanity check to know what files you have
    print_external_files(external_files);

    //Make initial PCB (notice how partition is not assigned yet)
    PCB current(0, -1, "init", 1, -1);
    //Update memory (partition is assigned here, you must implement this function)
    if(!allocate_memory(&current)) {
        std::cerr << "ERROR! Memory allocation failed!" << std::endl;
    }

    std::vector<PCB> wait_queue;

    /******************ADD YOUR VARIABLES HERE*************************/



    /******************************************************************/

    //Converting the trace file into a vector of strings.
    std::vector<std::string> trace_file;
    std::string trace;
    while(std::getline(input_file, trace)) {
        trace_file.push_back(trace);
    }

    auto [execution, system_status, _] = simulate_trace(trace_file, 
                                            0, 
                                            vectors, 
                                            delays,
                                            external_files, 
                                            current, 
                                            wait_queue);

    input_file.close();

    write_output(execution, "execution.txt");
    write_output(system_status, "system_status.txt");

    return 0;
}
