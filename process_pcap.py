#! /usr/bin/env python
#
# Analyzes the TCP flow(s) in a PCAP file and detects traffic policing.
#
# This engine dissects the packets stored in a PCAP file by first assigning each
# packet to a flow based on the 4-tuple (source/destination IP address and
# port). There is NO handling for 4-tuple reuse, e.g. when a connection is
# properly terminated and new connection is established using the same 4-tuple.
#
# Each flow is then divided into segments, where a segment is defined as data
# from endpoint A followed by data from endpoint B (i.e. a typical
# request/response pattern). The policing detection then runs on each segment,
# separately for each flow direction and produces one output line per execution
# using the following format:
#
# <input filename>,<flow index>,<segment index>,<direction>,<number of data
# packets>,<number of losses>,<policing results+>
#
# Direction is either "a2b" or "b2a"
# Policing results is composed of multiple columns, where the first two columns
# correspond to the analysis using all losses, and the other two columns
# correspond to the analysis ignoring the first and last two losses (i.e.
# cutoff=2).
#
# The policing results are structured as follows:
# <Is policed?>,[<result code>,<policing rate>,<data before first loss>]
#
# <Is policed?> is either "True" or "False"
# The result codes are defined in policing_detector.py
# Policing rate and data before first loss are "null" if no policing has been
# detected
#
# In a typical trace from the M-Lab NDT dataset only a single flow is captured
# with almost all data flowing from endpoint B (the server) to endpoint A (the
# client) indicating a single request/response pattern. Thus, the output
# generated usually includes two lines total (one per direction, there is only
# one segment) with policing only detectable for the server-to-client flow (i.e.
# direction "b2a").

import dpkt
import sys

from annotated_packet import *
from tcp_flow import *
from tcp_segment import *
from tcp_util import *

# Maximum number of packets that will be handled overall (NOT per flow)
MAX_NUM_PACKETS = -1

if len(sys.argv) < 2:
    print("Missing parameter(s)")
    print("Usage: python %s <input file>" % (sys.argv[0]))
    exit(-1)

input_filename = sys.argv[1]
input_file = open(input_filename, 'rb')
pcap = dpkt.pcap.Reader(input_file)

flows = dict()
index = 0
for ts, buf in pcap:
    eth = dpkt.ethernet.Ethernet(buf)

    try:
        # Convert TCP packet to an annotated version
        # This can fail, e.g. if the ethernet frame does not encapsulate a
        # IP/TCP packet
        ts_us = int(ts * 1E6)
        annotated_packet = AnnotatedPacket(eth, ts_us, index)
    except AttributeError:
        continue

    # Add packet to a flow based on the 4-tuple
    ip = annotated_packet.packet.ip
    key_1 = (ip.src, ip.dst, ip.tcp.sport, ip.tcp.dport)
    key_2 = (ip.dst, ip.src, ip.tcp.dport, ip.tcp.sport)
    if key_1 in flows:
        flows[key_1].add_packet(annotated_packet)
    elif key_2 in flows:
        flows[key_2].add_packet(annotated_packet)
    else:
        flows[key_1] = TcpFlow(annotated_packet)
        flows[key_1].add_packet(annotated_packet)

    # We are only looking the first thousand or so packets so we can abort
    # processing an excessive number of packets in the input file
    index += 1
    if MAX_NUM_PACKETS != -1 and index > MAX_NUM_PACKETS:
        break

input_file.close()

flow_index = 0
for _, flow in flows.items():
    flow.post_process()

    # Split flow into segments
    start_mode = end_mode = append_mode = False
    if flow_index == 0:
      start_mode = True
    if flow_index == len(flows.items()) - 1:
      end_mode = True
    if flow_index > 0 and flow_index < (len(flows.items()) - 1):
       append_mode = True
    segments = split_flow_into_segments(flow, append_mode,end_mode , start_mode)
    flow_index +=1
