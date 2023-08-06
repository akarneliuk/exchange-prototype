# Market Data
Market Data is an application, which distributes the actual data about what happens on exchange to all participants. The distribution is done via multicast so far with IPv4, but with possibility to add IPv6 support later. The application is written in C and uses POSIX sockets API. The application is designed to be run on Linux.

## Types of market data
*NOTE: This part will be continously updated*

Rather than invent the specification myself, for learning purposes, NASDAQ public specification is used:
- [Nasdaq Basic](https://data.nasdaq.com/databases/NB/documentation)
- [Nasdsq Last Sale](https://data.nasdaq.com/databases/NLS/documentation)

These market data will be distributed using the same application, but with different configuration.
Mapping of market data to multicast groups and ports is done in the following way:

| Market Data | Multicast Group | Multicast Port |
|---|---|---|
| Nasdaq Basic | 239.11.22.10 | 2210 |
| Nasdaq Last Sale | 239.11.22.11 | 2211 |


### Further plans
Later, it is planned to add support for:
- [Nasdaq QBBO](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/QBBOSpecification2.1.pdf)
- [Nasdaq TotalView ITCH 5.0](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf)