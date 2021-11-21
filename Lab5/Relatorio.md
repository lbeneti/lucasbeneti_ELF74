# Lab 5 - ThreadX

## azyre_rtos workspace
Tabela 1

Threads in sample_threadx.c
| Thread Name | entry function | stack size | priority | auto start | time slicing |
|-------------|----------------|------------|----------|------------|--------------|
| thread 0    | thread_0_entry |    1024    |    1     |    yes     |      no      |
| thread 1    | thread_1_entry |    1024    |   16     |    yes     |      sim (4) |
| thread 2    | thread_2_entry |    1024    |   16     |    yes     |      sim (4) |
| thread 3    | thread_3_and_4_entry |    1024    |    8     |    yes     |      no      |
| thread 4    | thread_3_and_4_entry |    1024    |    8     |    yes     |      no      |
| thread 5    | thread_5_entry |    1024    |    4     |    yes     |      no      |
| thread 6    | thread_6_and_7_entry |    1024    |    8     |    yes     |      no      |
| thread 7    | thread_6_and_7_entry |    1024    |    8     |    yes     |      no      |

Objects in sample_threadx.c
| Name         | Control Structure | Size | Location         |
|--------------|-------------------|------|------------------|
| byte pool 0  | byte_pool_0       | 9120 | byte_pool_memory |
| block pool 0 | block_pool_0      | 100  |                  |
| semaphore 0 | semaphore_0        | 32  |                  |
| mutex 0 | mutex_0        | 32  |                  |
| event flags 0 | event_flags_0        | 32  |                  |

## TODO:
** ainda falta o diagrama e completar a segunda tabela ** 
