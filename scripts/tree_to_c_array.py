import sys

# https://stackoverflow.com/a/5389547
def grouped(iterable, n):
   return zip(*[iter(iterable)] * n)

# Outputs result to stdout
def main():
   if len(sys.argv) != 2:
      sys.exit(f'Usage:\n{sys.argv[0]} raw_tree')

   with open(sys.argv[1], 'rb') as f:
      raw_data = f.read()

   assert len(raw_data) % 2 == 0

   data = []

   for low, high in grouped(raw_data, 2):
      value = (high << 8) | low
      data.append(value)

   array_data = str(data).replace('[', '{').replace(']', '}')
   print(array_data)

if __name__ == '__main__':
   main()
