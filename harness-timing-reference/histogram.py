import plotly.express as px
import numpy as np

filepath = 'intervals.data'

num_threads = -1 # how many threads
num_max = -1 # max number of records per thread
record_lengths = [] # how many records are there per thread?
records = {} # the actual data
data_encountered = False # have we read the line that says "data" yet?

with open(filepath) as fp:
	for line_num, line in enumerate(fp):
		if line_num == 0: # read number of threads
			num_threads = int(line.split(' ')[1])
			print("Number of threads: " + str(num_threads))
		elif line_num == 1: # read max records per thread
			num_max = int(line.split(' ')[1])
			print("Max records per thread: " + str(num_max))
		elif line_num == 2: # skip line
			continue
		elif line == "data\n": # set flag to start reading actual data
			data_encountered = True
		elif not data_encountered: # read length of records per thread
			tid = int(line.split(' ')[0])
			length = int(line.split(' ')[1])
			record_lengths.append(length)
			records[tid] = []
		else: # read data, omitting trailing 0s
			cur_tid = (int(line_num)-(4+num_threads)) // num_max

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

CPU_GHZ = 3.3 # rohiths cpu
all_intervals_usec = list(map(lambda x: cycles_to_usec(x, CPU_GHZ), all_intervals))
print("MEAN OF ALL INTERVALS: " + str(np.mean(all_intervals_usec)) + " usec")

THRESHOLD_USEC = 30
print("threshold " + str(THRESHOLD_USEC) + " cycles")

filtered_data = list(filter(lambda x: x == 0, all_intervals_usec))

print("number below threshold " + str(len(filtered_data)))
print("total data points " + str(len(all_intervals_usec)))
print("fraction is " + str(len(filtered_data)/len(all_intervals_usec)))

# fig = px.histogram(filtered_data)
# fig.show()
