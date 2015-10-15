import os
import sys
import hashlib

# tree node: (name, isdir, hash, [children])

def md5(filename):
	hash = hashlib.md5()
	with open(filename, "rb") as f:
		for chunk in iter(lambda: f.read(4096), b""):
			hash.update(chunk)
	return hash.hexdigest()

def construct_tree(path):
	name = os.path.basename(path)
	if os.path.isfile(path):
		return (name, False, md5(path), [])
	children = []
	for child in os.listdir(path):
		if child == '.svn':
			continue
		children.append(construct_tree(path + '/' + child))
	children = sorted(children, key = lambda x: x[0])
	return (name, True, None, children)

def normalize(path):
	path = path.replace('\\', '/')
	while path.find('//'):
		path = path.replace('//', '/')
	if path.endswith('/'):
		path = path[:-1]
	return path

def bool_to_type(b):
	if b:
		return 'dir'
	return 'file'

def compare_trees_helper(children_a, children_b, path):
	if len(children_a) != len(children_a):
		print("Path %s doesn't have the same number of children." % path)
		return False
	n = len(children_a)
	for i in range(n):
		if children_a[i][0] != children_b[i][0]:
			print('Path name mismatch: "%s" - "%s"' % (children_a[i][0], children_b[i][0]))
			return False
		if children_a[i][1] != children_b[i][1]:
			print('Type mismatch: "%s" (%s) - "%s" (%s)' % (children_a[i][0], bool_to_type(children_a[i][1]), children_b[i][0], bool_to_type(children_b[i][1])))
			return False
		if children_a[i][1]:
			continue
		if children_a[i][2] != children_b[i][2]:
			print('Hash mismatch: "%s" - "%s"' % (children_a[i][0], children_b[i][0]))
			return False
	for i in range(n):
		if not children_a[i][1]:
			continue
		if not compare_trees_helper(children_a[i][3], children_b[i][3], path + '/' + children_b[i][0]):
			return False
	return True

def compare_trees(A, B):
	return compare_trees_helper(A[3], B[3], '$(ROOT)')

def compare_tree_and_dir(A, B):
	second = construct_tree(B)
	return compare_trees(A, second)

def compare_directories(A, B):
	first = construct_tree(A)
	second = construct_tree(B)
	return compare_trees(first, second)

def main(args):
	if len(args) < 2:
		return
	return compare_directories(args[0], args[1])

print(main(sys.argv[1:]))
