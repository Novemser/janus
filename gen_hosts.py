with open('hosts-ipads.yml', 'w') as f:
  for i in range(0, 100):
    n = (i % 5) + 16
    f.write('  n' + str(i + 1) + ': val' + str(n) + '\n')
f.close() 
