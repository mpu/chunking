/* compile-time log2 */

#define LOG2_8(x) \
	(x < 1ul ? -1 : \
	 x < 2ul ? 0 : \
	 x < 4ul ? 1 : \
	 x < 8ul ? 2 : \
	 x < 16ul ? 3 : \
	 x < 32ul ? 4 : \
	 x < 64ul ? 5 : \
	 x < 128ul ? 6 : 7)
#define LOG2_16(x) (x < 256ul ? LOG2_8(x) : 8 + LOG2_8((x >> 8)))
#define LOG2_32(x) (x < 65536ul ? LOG2_16(x) : 16 + LOG2_16((x >> 16)))
