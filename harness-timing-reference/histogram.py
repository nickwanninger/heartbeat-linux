#!/usr/bin/python3

import plotly.express as px
import numpy as np

filepath = 'intervals.data'

num_threads = -1 # how many threads
num_max = -1 # max number of records per thread
record_lengths = [] # how many records are there per thread?
records = {} # the actual data

data_encountered = False # have we read the line that says "data" yet?
num_records_per_thread_encountered = False # same as above for "num_records_per_thread"

print("Reading file...\n")
with open(filepath) as fp:
	for line_num, line in enumerate(fp):

		if not data_encountered:
			sp = list(map(str.rstrip, line.split(' '))) # split and remove trailing newlines

			if sp[0] == "num_threads":
				num_threads = int(sp[1])
				print("Number of threads: " + str(num_threads))
			
			elif sp[0] == "timing_method":
				# DO SOMETHING WITH TIMING METHOD?
				print("Timing method: " + sp[1])
			
			elif sp[0] == "buffer_size_per_thread":
				num_max = int(sp[1])
				print("Max records per thread: " + str(num_max))
			
			elif sp[0] == "num_records_per_thread":
				num_records_per_thread_encountered = True
			
			elif num_records_per_thread_encountered:
				if sp[0] == "data":
					data_encountered = True
					continue

				tid = int(line.split(' ')[0])
				length = int(line.split(' ')[1])
				record_lengths.append(length)
				records[tid] = []
		else: # read data, omitting trailing 0s
			NUM_OF_TEXT_LINES = 5
			cur_tid = (int(line_num)-(NUM_OF_TEXT_LINES+num_threads)) // num_max

			if int(line_num) % num_max < record_lengths[cur_tid]:
				records[cur_tid].append(int(line))
print("\nDone reading file\n")
### WE'VE READ THE DATA, NOW PROCESS

def sanity_check(record_lengths):
	# ensure length of data read is correct
	for (tid, length) in enumerate(record_lengths):
		assert length <= num_max
		assert len(records[tid]) == length

def merge_threads_results(records):
	# merge results from ALL threads
	all_intervals = []
	for k in records:
		all_intervals += records[k]
	return all_intervals

def cycles_to_usec(cycles, cpu_ghz):
	cycles_per_usec = cpu_ghz * 1000
	return cycles / cycles_per_usec

def convert_cycles_list_to_usec_list(cycles_list, cpu_ghz):
	print("Converting cycles to usec with CPU speed: " + str(cpu_ghz) + " GHz\n")
	return list(map(lambda x: cycles_to_usec(x, cpu_ghz), cycles_list))

def print_statistics(data_list, unit):
	print("Mean: " + str(np.mean(data_list)) + " " + unit)
	print("Min:  " + str(np.min(data_list)) + " " + unit)
	print("Max:  " + str(np.max(data_list)) + " " + unit)
	print("Std dev:  " + str(np.std(data_list)) + " " + unit)
	print()

def threshold_list(data_list, low, high, unit):
	f1 = list(filter(lambda x: x <= high, data_list))
	f2 = list(filter(lambda x: x >= low, f1))
	n = len(f2)
	print("Total data points: " + str(len(data_list)))
	print("Data points between (both inclusive) " 
		+ str(low) + unit + " and " + str(high) + unit + ": " + str(n))
	print("Fraction is " + str(n/len(data_list)))

sanity_check(record_lengths)
all_intervals = merge_threads_results(records)

# Print number of zero data points to ensure data is not garbage
num_zeros = len(list(filter(lambda x: x == 0, all_intervals)))
print("Number of zero points: " + str(num_zeros))

# CPU_GHZ = 1.8 # rohiths cpu
CPU_GHZ = 3.3 # picard cpu
all_intervals_usec = convert_cycles_list_to_usec_list(all_intervals, CPU_GHZ)
print_statistics(all_intervals_usec, "usec")

THRESHOLD_LOW_USEC = 0
THRESHOLD_HIGH_USEC = 20
threshold_list(all_intervals_usec, THRESHOLD_LOW_USEC, THRESHOLD_HIGH_USEC, "usec")

fig = px.histogram(all_intervals_usec)
fig.update_layout(
    xaxis_title="interval (microseconds)",
    yaxis_title="count"
)
# fig.update_xaxes(tick0=0, dtick=10)
fig.show()
