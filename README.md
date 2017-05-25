# echo_benchmark

1kbyte echo network i/o benchmark

|Rps|Cpu|Memory|
|-|-|-|
|Node|10650.000000|95%|30M|
|Dotnet core|38150.148438|180%|10M (maybe multi-thread)|
|Boost asio|48332.101562|85%|400K|
|Uvloop|28439.199219|95%|10M|
