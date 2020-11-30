nums = list(map(int, open('chunks').read().split('\n')[0:-1]))
print("avg block length:", sum(nums) / len(nums))
