import dpkt
import json
from tcp_flow import *
      
class Packet():
    def __init__(self, annotated_packet):
        self.is_lost = annotated_packet.is_lost()
        self.timestamp_us = annotated_packet.timestamp_us
        self.ack_delay_ms = annotated_packet.ack_delay_ms
        self.index = annotated_packet.index
        self.seq_relative = annotated_packet.seq_relative
        self.data_length = annotated_packet.data_len
        self.ack_index = annotated_packet.ack_index;
        if annotated_packet.rtx is not None:
          self.hasRtx = False;
        else:
          self.hasRtx = True;

def split_flow_into_segments(flow, appendMode, finishMode, startMode):
    """Splits the flow into multiple segments where a segment is
    defined by: data only from endpoint A (request) followed by data only from
    endpoint B (response). New data from endpoint A initiates a new segment.
    Returns: list of TcpFlow instances with each instance representing a segment
    """
    segments = []
    if len(flow.packets) == 0:
        return segments

    current_sender = flow.endpoint_a
    current_segment = TcpFlow(flow.packets[0])
    segments.append(current_segment)
    for packet in flow.packets:
        # Non-data packets do not trigger a new segment
        if packet.data_len == 0:
            if len(current_segment.packets) > 0:
                current_segment.add_packet(packet, False)
            continue

        # If data comes from the other endpoint: enter response phase or create
        # a new segment
        ip = packet.packet.ip
        if current_sender.ip != ip.src or current_sender.port != ip.tcp.sport:
            if current_sender == flow.endpoint_a:
                current_sender = flow.endpoint_b
            else:
                current_sender = flow.endpoint_a
                current_segment = TcpFlow(packet)
                segments.append(current_segment)
        current_segment.add_packet(packet, False)

    f = open('data.json', 'a+')
    n = f.write("[")
    if startMode == True:
        n = f.write("[")
    f.close()
    f2 = open('data2.json', 'a+')
    n = f2.write("[")
    if startMode == True:
        n = f2.write("[")
    f2.close()
    print("length of segments %d",len(segments))
    for ob in segments:
          newPacketList = []

          for p in ob.endpoint_a.packets:
              newPacket = Packet(p)
              newPacketList.append(newPacket)
          json_string = json.dumps([obj.__dict__ for obj in newPacketList])
          f = open('data.json', 'a+')
          n = f.write(json_string)
          n = f.write(",")
          f.close()

          newPacketList = []

          f2 = open('data2.json','a+')
          json_median_rtt_string = "{\"median_rtt\":" + str(ob.endpoint_a.get_median_rtt_ms()) + "}"
          n = f2.write(json_median_rtt_string)
          n = f2.write(",")
          f2.close()

          print(len(ob.endpoint_a.packets))
          print(len(ob.endpoint_b.packets))
          for p in ob.endpoint_b.packets:
              newPacket = Packet(p)
              newPacketList.append(newPacket)
          json_string = json.dumps([obj.__dict__ for obj in newPacketList])
          f = open('data.json', 'a+')
          n = f.write(json_string)
          if (ob != segments[len(segments) - 1]):
              n = f.write(",")
          f.close()

          f2 = open('data2.json','a+')
          json_median_rtt_string = "{\"median_rtt\":" + str(ob.endpoint_b.get_median_rtt_ms()) + "}"
          n = f2.write(json_median_rtt_string)
          if (ob != segments[len(segments) - 1]):
             n = f2.write(",")
          f2.close()

    f = open('data.json', 'a+')
    n = f.write("]")
    if finishMode == False:
        n = f.write(",")
    if finishMode == True:
        n = f.write("]")

    f.close()
    f2 = open('data2.json', 'a+')
    n = f2.write("]")
    if finishMode == False:
        n = f2.write(",")
    if finishMode == True:
        n = f2.write("]")
    f2.close()
    return segments
