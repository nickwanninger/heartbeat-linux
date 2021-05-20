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
		elif idx == 1: # read max records per thread
			num_max = int(line.split(' ')[1])
		elif idx ==2: # skip line
			continue
		elif line == "data\n": # set flag to start reading actual data
			data_encountered = True
		elif not data_encountered: # read length of records per thread
			tid = int(line.split(' ')[0])
			length = int(line.split(' ')[1])
			record_lengths.append(length)
			records[tid] = []
		else: # read data, omitting trailing 0s
			cur_tid = (int(idx)-6) // num_max;

			if int(idx) % num_max < record_lengths[cur_tid]:
				records[cur_tid].append(int(line))

# sanity check, ensure length of data read is correct
for (tid, length) in enumerate(record_lengths):
	assert len(records[tid]) == length

fig = px.histogram(records[0])
fig.show()