Carry out the execution in the following manner:
1.rm -rf data.json
2.rm -rf data2.json
3.python process_pcap.py final.pcap
4.cat data.json | jq '.' > datajs.json
5.cat data2.json | jq '.' > data2js.json
6.g++ policingDetector.cpp
7. ./a.out
