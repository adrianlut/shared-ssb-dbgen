# Shared Star Schema Benchmark data set generator (shared-ssb-dbgen)

This repository holds the data generation utility for the Shared Star Schema Benchmark (SSB) for shared DWH analytics.
It generates schema data as table files, in a simple textual format, which can then be loaded into a DBMS for running
the benchmark.

The Shared Start Schema Benchmark extends the original SSB to a data sharing setting where multiple merchants operate a
joint data warehouse. In this data warehouse, some of the customers, suppliers and parts overlap (are duplicated). The
code additions I made to `dbgen` enable the user to specify the distribution of orders, customers, suppliers, and parts
between merchants. The distributions also control the amount of duplicated rows.

This is a fork of the Star Schema Benchmark (SSB) repository of [eyalroz](https://github.com/eyalroz). If your are
looking for the original SSB with some extensions and fixes, look there.

## <a name="building">Building the generation utility</a>

The build is automated using [CMake](https://cmake.org/) now. You can run it in several modes:

* Default: `$ cmake -S . -B ./build && cmake --build ./build`
* Passing options manually: `&& cmake -S . -B ./build [OPTIONS] && cmake --build ./build`
* Interactive: `$ && cmake -S . -B ./build && ccmake -S . -B ./build && cmake --build ./build`

Of course, you should have C language compiler (C99/C2011 support is not necessary), linker, and corresponding make-tool
preinstalled in your system. CMake will detect them automatically.

#### Available options

| Option              | What is it about?                                                                                                                                                                   | Possible values                                  | Default value | Tested with SSSB? |
|---------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|--------------------------------------------------|---------------|-------------------|
| `CMAKE_BUILD_TYPE`  | Predefined CMake option. if `Debug`, then build with debugging symbols and without optimizations; if `Release`, then build with optimizations.                                      | `Release`, `Debug`                               | `Release`     | Yes               |
| `DATABASE`          | DBMS which you are going to benchmark with SSB. This option only affects `qgen`, so if you're only generating data, let it stay at its default value .                              | `INFORMIX`, `DB2`, `TDAT`, `SQLSERVER`, `SYBASE` | `DB2`         | No                |
| `EOL_HANDLING`      | If `ON`, then separator is omitted after the last column in all tables.                                                                                                             | `ON`  `OFF`                                      | `OFF`         | Only ON           |
| `CSV_OUTPUT_FORMAT` | Adhere to the CSV format for the output, i.e. use commans (`,`) as a field separator, and enclose strings in double-quotes to ensure any commas within them aren't mis-interpreted. | `ON` `OFF`                                       | `OFF`         | No                |
| `WORKLOAD`          | Workload is always SSB!                                                                                                                                                             | ---                                              | ---           | Yes               |
| `YMD_DASH_DATE`     | When set to `ON`, generates dates with dashes between fields, i.e. `YYYY-MM-DD`; when set to `OFF`, no dashes are included, e.g. `YYYYMMDD`                                         | `ON`, `OFF`                                      | `OFF`         | Only OFF          |

## <a name="using">Using the utility to generate data</a>

The `dbgen` utility should be run from within the source folder (it can be run from elsewhere but you would need to
specify the location of the `dists.dss` file). A typical invocation:

    $ ./dbgen -v -s 10

will create all tables in the current directory, with a scale factor of 10. This will have, for example, 300,000 lines
in `customer.tbl`, beginning with something like:

```
1|Customer#000000001|j5JsirBM9P|MOROCCO  0|MOROCCO|AFRICA|25-989-741-2988|BUILDING|0|
2|Customer#000000002|487LW1dovn6Q4dMVym|JORDAN   1|JORDAN|MIDDLE EAST|23-768-687-3665|AUTOMOBILE|0|
3|Customer#000000003|fkRGN8n|ARGENTINA7|ARGENTINA|AMERICA|11-719-748-3364|AUTOMOBILE|0|
4|Customer#000000004|4u58h f|EGYPT    4|EGYPT|MIDDLE EAST|14-128-190-5944|MACHINERY|0|
```

the fields are separated by a pipe character (`|`), and if `EOL_HANDLING`was set to `OFF` during building there's a
trailing separator at the end of the line.

After generating `.tbl` files for the CUSTOMER, PART, SUPPLIER, DATE, and LINEORDER tables, you should now either load
them directly into your DBMS or apply some textual processing to them before loading.

**Note:** On Unix-like systems, it is also possible to write the generated data into a FIFO filesystem node, reading
from the other side with a compression utility, so as to only write compressed data to disk. This may be useful if disk
space is limited and you are using a particularly high scale factor.
