# Virtual Memory Simulation Using Demand Paging

## Overview
This project simulates a demand-paged virtual memory system. Processes generate virtual addresses, which the Memory Management Unit (MMU) translates into physical addresses. If a page is not in memory, a page fault occurs, and the MMU retrieves the page from disk (simulated), possibly replacing an existing page using the Least Recently Used (LRU) algorithm. The simulation consists of the following four modules:

- **Master**: Initializes data structures and creates other modules.
- **Scheduler**: Schedules processes using the First-Come, First-Served (FCFS) algorithm.
- **MMU**: Translates page numbers to frame numbers and handles page faults.
- **Process**: Generates page numbers from a reference string.

## Input
- **Total number of processes (k)**
- **Virtual address space (m)**: Maximum number of pages required per process.
- **Physical address space (f)**: Total number of frames.

## Modules

### Master Process
**Tasks**:
- **Data Structures**:
  - **Page Table**: One per process, with a size equal to the virtual address space.
  - **Free Frame List**: Shared memory list used by the MMU.
  - **Ready Queue**: Message queue used by the scheduler.
  - **Communication Queues**: Two message queues for scheduler-MMU and process-MMU communication.

**Process Creation**:
- Creates the scheduler, MMU, and processes at fixed intervals.
- Generates page reference strings for processes.

**Implementation**:
- **File**: `Master.c`
- Initializes data structures and creates the required modules.
- Waits for the scheduler to notify termination of the modules.

**Reference String Generation**:
- Generates a random-length reference string, ensuring some invalid addresses.
- Handles potential illegal addresses causing segmentation faults.

### Scheduler
**Tasks**:
- Schedules processes from the ready queue using the FCFS algorithm.
- Handles messages from the MMU:
  - **PAGE FAULT HANDLED**: Enqueues the process.
  - **TERMINATED**: Schedules the next process.

**Implementation**:
- **File**: `sched.c`
- Executed by the Master process, manages process execution and termination.

### Processes
**Tasks**:
- Generates page numbers from the reference string and interacts with the MMU.
- Handles valid frame numbers, page faults, and invalid page references.
- Notifies the MMU upon completion.

**Implementation**:
- **File**: `process.c`
- Created by the Master process, waits in the ready queue until scheduled.

### Memory Management Unit (MMU)
**Tasks**:
- Translates page numbers to frame numbers.
- Handles page faults by invoking the Page Fault Handler (PFH).
- Sends messages to the scheduler based on page fault handling and process completion.

**Implementation**:
- **File**: `MMU.c`
- Executed by the Master process, maintains a global timestamp for page references.

## Data Structures
- **Page Table**: Shared memory, one per process, including frame number and validity bit.
- **Free Frame List**: Shared memory list managed by the MMU.
- **Process to Page Number Mapping**: Shared memory, contains the number of pages per process.

## Output
- **MMU**: Logs page fault information, invalid page references, and global ordering.
- **Output File**: `result.txt` contains all logs.

---

This simulation replicates the behavior of a demand-paged virtual memory system, demonstrating process management, scheduling, and page management to provide insight into the complexities of operating system memory management.
