import sys, json

INFILE = sys.argv[1]
PREFIX = sys.argv[2]  # e.g "build/obj/dev/"

with open(INFILE, "r") as f:
  compdb = json.load(f)

compdb = [e for e in compdb if e["output"].startswith(PREFIX)]


# json.dumps([1, 2, 3, {'4': 5, '6': 7}], separators=(',', ':'))
print(json.dumps(compdb, sort_keys=True, indent=2))
