## 26 Трассировка системных вызовов

### Сборка

Для тестов использовалась следующая операционная система:
```bash
# lsb_release -a
No LSB modules are available.
Distributor ID:	Debian
Description:	Debian GNU/Linux 13 (trixie)
Release:	13
Codename:	trixie
# uname -a
Linux debian13 6.12.74+deb13+1-arm64 #1 SMP Debian 6.12.74-2 (2026-03-08) aarch64 GNU/Linux
```

Сборка трассируемой программы 
```bash
gcc -o ftrace_demo ftrace_demo.c
```

### Выполнение трассировки

Запуск трассируемой программы

```bash
$ ./ftrace_demo
=== FTRACE SYSCALL DEMO ===
PID: 1336

COPY THE PID NUMBER ABOVE!
Set up ftrace in another terminal, then press Enter...
Waiting for Enter:
```

Запуск трассировки
```bash
$ sudo su -
[sudo] password for cutter:
# cd /sys/kernel/debug/tracing/
# ls
available_events		  dynamic_events	 max_graph_depth      set_event_notrace_pid   stack_max_size	  trace_marker_raw
available_filter_functions	  dyn_ftrace_total_info  options	      set_event_pid	      stack_trace	  trace_options
available_filter_functions_addrs  enabled_functions	 per_cpu	      set_ftrace_filter       stack_trace_filter  trace_pipe
available_tracers		  error_log		 printk_formats       set_ftrace_notrace      synthetic_events	  tracing_cpumask
buffer_percent			  events		 README		      set_ftrace_notrace_pid  timestamp_mode	  tracing_max_latency
buffer_size_kb			  free_buffer		 saved_cmdlines       set_ftrace_pid	      touched_functions   tracing_on
buffer_subbuf_size_kb		  instances		 saved_cmdlines_size  set_graph_function      trace		  tracing_thresh
buffer_total_size_kb		  kprobe_events		 saved_tgids	      set_graph_notrace       trace_clock	  uprobe_events
current_tracer			  kprobe_profile	 set_event	      snapshot		      trace_marker	  uprobe_profile
# echo 0 > tracing_on
# echo > trace
# echo function > current_tracer
# echo '__arm64_sys_*' > set_ftrace_filter
# echo 1336 > set_ftrace_pid
# echo 1 > tracing_on
```

Продолжение выполнения трассируемой программы
```bash
...
Waiting for Enter:

=== SYSCALL OPERATIONS START ===
1. Opening file (openat syscall)...
   File opened, fd = 3
2. Writing to file (write syscall)...
   Written 32 bytes
3. Getting file info (newfstat syscall)...
   File size: 32 bytes
   File permissions: 644
4. Closing file (close syscall)...
   File closed
5. Deleting file (unlink syscall)...
   File deleted

=== SYSCALL OPERATIONS END ===
Check /tmp/ftrace_output.txt for syscall traces!
```

Завершение трассировки
```bash
# echo 0 > tracing_on
# cat trace > /tmp/ftrace_output.txt
# echo > set_ftrace_pid
```

### Результаты

```bash
# cat /tmp/ftrace_output.txt
# tracer: function
#
# entries-in-buffer/entries-written: 22/22   #P:2
#
#                                _-----=> irqs-off/BH-disabled
#                               / _----=> need-resched
#                              | / _---=> hardirq/softirq
#                              || / _--=> preempt-depth
#                              ||| / _-=> migrate-disable
#                              |||| /     delay
#           TASK-PID     CPU#  |||||  TIMESTAMP  FUNCTION
#              | |         |   |||||     |         |
     ftrace_demo-1336    [000] .....   690.804475: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804492: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804494: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804504: __arm64_sys_openat <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804687: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804704: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804713: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804745: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804751: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804754: __arm64_sys_newfstat <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804758: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804759: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804759: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804762: __arm64_sys_close <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804770: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804770: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804773: __arm64_sys_unlinkat <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804811: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804815: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804816: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804816: __arm64_sys_write <-invoke_syscall
     ftrace_demo-1336    [000] .....   690.804846: __arm64_sys_exit_group <-invoke_syscall
```

В сохраненной трассировке видны системные вызовы и время с момента старта 
системы:
* openat()   - открытие/создание файла
* write()    - запись данных в файл
* newfstat() - получение информации о файле
* close()    - закрытие файлового дескриптора
* unlink()   - удаление файла
