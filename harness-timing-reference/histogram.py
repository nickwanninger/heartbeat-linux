import plotly.express as px
import numpy as np

filepath = 'intervals.data'

num_threads = -1 # how many threads
num_max = -1 # max number of records per thread
record_lengths = [] # how many records are there per thread?
records = {} # the actual data

data_encountered = False # have we read the line that says "data" yet?
num_records_per_thread_encountered = False # same as above for "num_records_per_thread"

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
			
			elif sp[0] == "max_records_per_thread":
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

# sanity check, ensure length of data read is correct
for (tid, length) in enumerate(record_lengths):
	assert length <= num_max
	assert len(records[tid]) == length

# merge results from ALL threads
all_intervals = []
for k in records:
	all_intervals += records[k]

#### WE READ ALL DATA, NOW PROCESS

def cycles_to_usec(cycles, cpu_ghz):
	cycles_per_usec = cpu_ghz * 1000
	return cycles / cycles_per_usec

CPU_GHZ = 1.8 # rohiths cpu
all_intervals_usec = list(map(lambda x: cycles_to_usec(x, CPU_GHZ), all_intervals))
print("MEAN OF ALL INTERVALS: " + str(np.mean(all_intervals_usec)) + " usec")

THRESHOLD_USEC = 30
print("threshold " + str(THRESHOLD_USEC) + " cycles")

filtered_data = list(filter(lambda x: x < THRESHOLD_USEC, all_intervals_usec))

print("number below threshold " + str(len(filtered_data)))
print("total data points " + str(len(all_intervals_usec)))
print("fraction is " + str(len(filtered_data)/len(all_intervals_usec)))

fig = px.histogram(all_intervals)
fig.show()
