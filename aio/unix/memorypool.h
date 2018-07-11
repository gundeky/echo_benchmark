#ifndef __MEMORYPOOL_H_29E4A4DC_4182_4EB9_81A5_0AE1B28FE597__
#define __MEMORYPOOL_H_29E4A4DC_4182_4EB9_81A5_0AE1B28FE597__

#include <stdlib.h>
#include <assert.h>
#include <vector>

#ifdef ENABLE_CHUNK
#ifndef CHUNK_SIZE
#define CHUNK_SIZE 100
#endif // CHUNK_SIZE
#endif // ENABLE_CHUNK

namespace aio {

class pool_base
{
	public:
		~pool_base()
		{
			//printf("pool.size : %zd\n", _chunks.size());    // for debug
			for(size_t i=0; i<_chunks.size(); ++i)
				free(_chunks[i]);
		}

		void* acquire(size_t size)
		{
			void* ret;
			if(_free.size() != 0)
			{
				ret = _free.back();
				_free.pop_back();
			}
			else
			{
#ifdef ENABLE_CHUNK
				ret = malloc(size * CHUNK_SIZE);
				_chunks.push_back(ret);
				for (int i=1; i<CHUNK_SIZE; ++i)
				{
					_free.push_back( ((char*)ret) + (i * size) );
				}
#else
				ret = malloc(size);
				_chunks.push_back(ret);
#endif
			}
			return ret;
		}

		void release(void* ptr)
		{
			_free.push_back(ptr);
		}

		size_t pool_size() { return _chunks.size(); }
		size_t free_size() { return _free.size(); }

	private:
		std::vector<void*> _chunks;   // all alloced instances
		std::vector<void*> _free;     // free instances
};

template <typename T>
class memory_pool
{
	public:
		static void* operator new(size_t size)
		{
			assert(sizeof(T) == size);
			return _base.acquire(size);
		}

		static void operator delete(void* ptr)
		{
			_base.release(ptr);
		}

	private:
		static pool_base _base;
};

// 여러 파일에서 include 될때 아래 부분 문제 없는지 확인
template <typename T>
pool_base memory_pool<T>::_base;

}    // end of namespace aio

#endif /*__MEMORYPOOL_H_29E4A4DC_4182_4EB9_81A5_0AE1B28FE597__*/
