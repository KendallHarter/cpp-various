import sys
import json

# https://stackoverflow.com/a/5389547
def grouped(iterable, n):
   return zip(*[iter(iterable)] * n)

def emit_byte(iterable, file_handle):
   to_write = 0
   for val in iterable:
      to_write <<= 1
      if val:
         to_write += 1
   file_handle.write(to_write.to_bytes(1, 'little'))

def main():
   if len(sys.argv) != 4:
      sys.exit(f'Usage:\n{sys.argv[0]} json_encoding file_to_compress file_to_save_to')

   with open(sys.argv[1]) as f:
      tree = json.load(f)

   with open(sys.argv[2], 'br') as f:
      data = f.read()

   # Wow this is bad/lazy but who cares
   to_emit = ''

   for d in (str(x) for x in data):
      to_emit += tree[d]

   with open(sys.argv[3], 'bw') as f:
      for byte in grouped(to_emit, 8):
         emit_byte(byte, f)

      remaining = len(to_emit) % 8
      last_byte = to_emit[-remaining:]

      while len(last_byte) != 8:
         last_byte += '0'

      emit_byte(last_byte, f)


if __name__ == '__main__':
   main()
