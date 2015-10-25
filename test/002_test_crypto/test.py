import os
import sys
import random
import compare_dirs
import pickle

data_base_path = os.getcwd()
back_base_path = os.getcwd()
backup_dst = back_base_path + '\\backup'
script_name = 'backup_script.txt'
script_path = data_base_path + '\\' + script_name

max_line_length = 120
min_line_length = max_line_length / 10

def to_native_slash(s):
	return s.replace('/', '\\')
	#return s

def to_memory_slash(s):
	return s.replace('\\', '/')
	#return s

def load_file(path):
	return [s.replace('\n', '') for s in open(path, 'r').readlines()]

def load_list():
	return load_file('..\\wordsEn.txt')

wordlist = load_list()

def pick_word():
	return random.choice(wordlist)

def generate_random_line(override_length = -1):
	ret = ''
	length = random.randint(0, max_line_length)
	if override_length >= 0:
		length = override_length
	if length >= min_line_length or override_length >= 0:
		while len(ret) < length:
			if len(ret) > 0:
				ret += ' '
			ret += pick_word()
	return ret

def generate_new_file(path):
	file = open(path, 'w')
	ret = ''
	lines = random.randint(20, 512)
	for i in range(lines):
		file.write(generate_random_line() + '\n')

def edit_line(line):
	words = line.split(' ')
	if '' in words:
		words.remove('')
	if len(words) > 1:
		start = random.randint(0, len(words) - 1)
		length = min(len(words) - start, random.randint(1, int(len(words) / 2)))
	else:
		start = 0
		length = len(words)
	for i in range(start, start + length):
		words[i] = pick_word()
	return ' '.join(words)

def edit_existing_file(path):
	lines = load_file(path)
	for i in range(random.randint(0, 5)):
		blockstart = random.randint(0, len(lines) - 1)
		r = random.randint(1, 100)
		count = min(len(lines) - blockstart, r)
		for j in range(blockstart, blockstart + count):
			#if j >= len(lines):
			#	print('%d, %d, %d, %d'%(len(lines), blockstart, r, count))
			lines[j] = edit_line(lines[j])
	
	if random.randint(0, 3) == 0:
		new_lines_count = random.randint(20, 512)
		for i in range(new_lines_count):
			lines.append(generate_random_line())
	
	file = open(path, 'w')
	for line in lines:
		file.write(line + '\n')

def exponential_distribution(k = 2):
	return (k ** random.uniform(1, 10)) / (k ** 10)

def generate_new_binaries_batch(base, addenda):
	for i in range(random.randint(1, 20)):
		path = 'binary.files/%08d.bin'%i
		addenda.append(path)
		file = open(base + '/' + path, 'wb')
		buffer = bytearray(os.urandom(int(exponential_distribution() * 1e+8)))
		file.write(buffer)

def generate_next_version(base, directories, files):
	addenda = []
	if len(files) == 0:
		for i in range(random.randint(0, 10)):
			path = pick_word() + '.txt'
			files.add(path)
			addenda.append(path)
			generate_new_file(base + '/' + path)
		os.mkdir('%s/binary.files'%base)
		addenda.append('binary.files')
		#generate_new_binaries_batch(base, addenda)
	else:
		for i in range(min(random.randint(1, 100), len(files))):
			path = random.sample(files, 1)[0]
			edit_existing_file(base + '/' + path)
		
		if random.randint(1, 25) == 1:
			for i in range(random.randint(1, 5)):
				container = None
				if random.randint(0, len(directories)) == 0:
					container = ''
				else:
					container = random.sample(directories, 1)[0] + '/'
				path = container + pick_word()
				directories.add(path)
				addenda.append(path)
				os.mkdir(base + '/' + path)
		
		if random.randint(1, 5) == 1:
			for i in range(random.randint(1, 5)):
				container = None
				if random.randint(0, len(directories)) == 0:
					container = ''
				else:
					container = random.sample(directories, 1)[0] + '/'
				path = container + pick_word() + '.txt'
				files.add(path)
				addenda.append(path)
				generate_new_file(base + '/' + path)
		
		if random.randint(1, 50) == 1:
			generate_new_binaries_batch(base, addenda)
	
	return

def delete_directory(dir):
	os.system('rd /q /s "%s"'%(to_native_slash(dir)))

def isalpha(x):
	return x >= 'a' and x <= 'z' or x >= 'A' and x <= 'Z'

def simple_wd():
	path = os.getcwd()
	print(path)
	if len(path) > 2 and isalpha(path[0]) and path[1] == ':':
		path = path[2:]
	print(path)
	return to_memory_slash(path)

def generate_backup_script(base):
	cd = os.getcwd()
	script  = 'open %s\n' % backup_dst
	script += 'add %s\\%s\n' % (cd, base)
	script += 'exclude name dirs .svn\n'
	script += 'set change_criterium date\n'
	script += 'set use_snapshots false\n'
	script += 'select keypair key.dat\n'
	script += 'backup\n'
	script += 'quit\n'
	open(script_path, 'w').write(script)

def initialize(base):
	delete_directory(base)
	os.mkdir(base)
	os.system('rd /q /s %s\\backup' % back_base_path)
	generate_backup_script(base)

def save_version(base):
	return compare_dirs.construct_tree(base)

def perform_backup():
	os.system('zekvok < %s > nul' % script_path)

def print_percent(i, width, n):
	percent = int(i * width / n)
	invpercent = width - percent
	sys.stdout.write('\r[%s%s] %d'%('#'*percent, ' '*invpercent, i))

def perform(base, version_count):
	initialize(base)
	directories = set()
	files = set()
	version_data = {}
	for i in range(version_count):
		print_percent(i, 80, version_count)
		generate_next_version(base, directories, files)
		version_data[i] = save_version(base)
		perform_backup()
	return version_data

def generate_data(version_count):
	filename = 'revision_data.pickle'
	try:
		file = open(filename, 'rb')
		return pickle.load(file)
	except:
		pass
	data = perform('test_repo', version_count)
	pickle.dump(data, open(filename, 'wb'))
	return data

def restore_backup(version):
	script  = 'open %s\\backup\n'
	script += 'select version %d\n'
	script += 'select keypair key.dat 123456\n'
	script += 'restore\n'
	script += 'quit\n'

	cd = os.getcwd()
	script = script % (data_base_path, version)
	open('restore_script.txt', 'w').write(script)
	os.system('zekvok < %s\\restore_script.txt > nul' % cd)

def perform_test(data, version_count):
	for i in range(version_count):
		print_percent(i, 80, version_count)
		restore_backup(i)
		if not compare_dirs.compare_tree_and_dir(data[i], 'test_repo'):
			return False
	return True

def test():
	os.system('copy /y ..\\..\\bin64\\zekvok.exe .')
	if not os.path.isfile('key.dat'):
		open('generate_key.txt', 'w').write(
			'generate keypair test key.dat 123456\n' +
			'quit\n'
		)
		os.system('zekvok < %s\\generate_key.txt > nul' % os.getcwd())
	version_count = 100
	data = generate_data(version_count)
	result = perform_test(data, version_count)
	print('')
	if result:
		print('Hooray! No errors found!')
	else:
		print('Some error(s) found in the program.')
	return

test()
