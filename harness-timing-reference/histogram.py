import plotly.express as px

filepath = 'intervals.data'

num_threads = -1 # how many threads
num_max = -1 # max number of records per thread
record_lengths = [] # how many records are there per thread?
records = {} # the actual data
data_encountered = False # have we read the line that says "data" yet?

with open(filepath) as fp:
	for idx, line in enumerate(fp):
		if idx == 0: # read number of threads
			num_threads = int(line.split(' ')[1])
			print("Number of threads: " + str(num_threads))
		elif idx == 1: # read max records per thread
			num_max = int(line.split(' ')[1])
			print("Max records per thread: " + str(num_max))
		elif idx == 2: # skip line
			continue
		elif line == "data\n": # set flag to start reading actual data
			data_encountered = True
		elif not data_encountered: # read length of records per thread
			tid = int(line.split(' ')[0])
			length = int(line.split(' ')[1])
			record_lengths.append(length)
			records[tid] = []
		else: # read data, omitting trailing 0s
			cur_tid = (int(idx)-(4+num_threads)) // num_max

			if int(idx) % num_max < record_lengths[cur_tid]:
				records[cur_tid].append(int(line))

# sanity check, ensure length of data read is correct
for (tid, length) in enumerate(record_lengths):
	assert length <= num_max
	assert len(records[tid]) == length

# merge results from ALL threads
all_intervals = []
for k in records:
	all_intervals += records[k]

NUM_CYCLES_PER_USEC = 1.8 * 10**3 # rohiths CPU is 1.8 GHz
USECS = 100 # microseconds
threshold = NUM_CYCLES_PER_USEC * USECS # in cycles
print("threshold " + str(threshold) + " cycles")

k = list(filter(lambda x: x > threshold, all_intervals))
num_over_thr = len(k)
print("result " + str(num_over_thr))

num_all_records = len(all_intervals)
print("total num records " + str(num_all_records))
print("fraction is " + str(num_over_thr/num_all_records))

fig = px.histogram(k)
fig.show()