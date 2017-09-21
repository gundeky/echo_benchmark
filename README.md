# echo_benchmark

1kbyte echo network i/o benchmark

||Rps|Cpu|Memory|
|-|-|-|-|
|Boost asio|158273|~~100%~~|~~400K~~|
|Boost asio coro|153344|~~100%~~|~~400K~~|
|Java nio|152205|~~95%~~|~~10M~~|
|Kotlin nio|153199|~~95%~~|~~10M~~|
|Netty|103745|~~95%~~|~~10M~~|
|Uvloop|61100|~~95%~~|~~10M~~|
|Node|48832|~~95%~~|~~30M~~|
|~~Dotnet core~~|~~38150.148438~~|~~180%~~|~~10M (maybe multi-threaded)~~|
