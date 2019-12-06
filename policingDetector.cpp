#include<iostream>
#include<string.h>
#include<vector>
#include <fstream>
#include <math.h>

#define RESULT_POLICED 0
#define RESULT_INSUFFICIENT_LOSS 1
#define RESULT_NEGATIVE_FILL 2
#define ZERO_THRESHOLD_LOSS_RTT_MULTIPLIER 2.00
#define ZERO_THRESHOLD_PASS_RTT_MULTIPLIER 2.75
#define LATE_LOSS_THRESHOLD pow(10,6)*2
#define RESULT_LATE_LOSS 3
#define RESULT_HIGHER_FILL_ON_LOSS 4
#define RESULT_LOSS_FILL_OUT_OF_RANGE 5
#define RESULT_PASS_FILL_OUT_OF_RANGE 6
#define RESULT_INFLATED_RTT  7
#define INFLATED_RTT_THRESHOLD 1.3
#define INFLATED_RTT_PERCENTILE 10
#define INFLATED_RTT_TOLERANCE 0.2
#define ZERO_THRESHOLD_LOSS_OUT_OF_RANGE 0.10
#define MIN_NUM_SLICES_WITH_LOSS 3
#define MIN_NUM_SAMPLES 15
#define ZERO_THRESHOLD_PASS_OUT_OF_RANGE 0.03

using namespace std;

class Packet{
	private :
	 bool is_lost;
	 long int timestamp_us;
	 float ack_delay_ms;
	 int index;
	 int datalength;
	 bool hasRtx;
	 int ack_index;
	 long int seq_relative;

    public:
    	bool getIsLost()
    	{
    		return is_lost;
    	}
    	long int getTimestampUs()
    	{
    		return timestamp_us;
    	}
    	long int getSeqRelative()
    	{
    		return seq_relative;
    	}
    	float getAckDelay()
    	{
    		return ack_delay_ms;
    	}
    	int getIndex()
    	{
    		return index;
    	}
    	int getDataLength()
    	{
             return datalength;
    	}
    	int getAckIndex()
    	{
             return ack_index;
    	}

    	void setIsLost(bool isLost)
    	{
    		this->is_lost = isLost;
    	}
    	void setSeqRelative(long int s)
    	{
    		this->seq_relative = s;
    	}
    	void setTimestampUs(long int timestamp_us)
    	{ 
           this->timestamp_us = timestamp_us;
    	}
    	void setAckDelay(long int ack_index)
    	{ 
           this->ack_index = ack_index;
    	}
        void setAckDelay(float x)
        {
           this->ack_delay_ms = x;
        }
        void setIndex(int d)
        {
            this->index = d;
        }
        void setDataLength(int d)
        {
            this->datalength = d;
        }
        
        void setHasRtx(bool rtx)
        {
        	this->hasRtx = rtx;
        }

    	bool getHasRtx()
    	{
    		return hasRtx;
    	}

};

class Endpoint{
private:
	vector<Packet> packets;
	float medianRttus;
public:
	vector<Packet> getPackets()
	{
		return this->packets;
	}
	void setPacketList(vector<Packet> packets)
	{
		this->packets = packets;
	}

	void setMedianRtt(float median_rtt_us){
         this->medianRttus = median_rtt_us;
	}
	float getMedianRtt()
	{
		return this->medianRttus;
	}
};

class Flow{
	vector<Endpoint> segArray;
	int flowIndex;
   public:
   	void setEndpointList(vector<Endpoint> elist)
   	{
   		this->segArray = elist;
   	}
   	vector<Endpoint> getEndpointList()
   	{
   		return segArray;
   	}

};

class PolicingParams {
	private :
	int result_code;
	int policing_rate_bps;
	int burst_size;
        int index;

    public : 
    PolicingParams(int code,int policing_rate_bps,int burst_size)
    {
    	this->result_code = code;
    	this->policing_rate_bps = policing_rate_bps;
    	this->burst_size = burst_size;
    	cout<<"result code: "<<code<<" "<<", policing_rate: "<<" "<<policing_rate_bps<<" "<<",burst_size: "<<burst_size<<endl;
    }
    PolicingParams(int code)
    {
    	this->result_code = code;
    	this->policing_rate_bps = -1;
    	this->burst_size = -1;
    	cout<<"result code: "<<code<<", "<<"policing_rate:"<<policing_rate_bps<<",burst_size: "<<burst_size<<endl;
    }
	void setResultcode(int result_code)
	{
		this->result_code = result_code;
	}
	int getResultCode()
	{
		return this->result_code;
	}
	int setPolicingRateBPS(int policing_rate_bps)
	{
		this->policing_rate_bps = policing_rate_bps;
	}

};

int goodPutBetweenFirstAndLastLoss(Endpoint endpoint, Packet *first_packet, Packet *last_packet){
    //Computes the goodput (in bps) achieved between observing two specific packets
    if(first_packet->getIndex() == last_packet->getIndex() || first_packet->getTimestampUs() == last_packet->getTimestampUs()){
        return 0;
    }

    int byte_count = 0;
    bool seen_first = false;
    vector<Packet> pset = endpoint.getPackets();
    for(int i =0;i < pset.size();i++)
    {
        if(pset[i].getIndex() == last_packet->getIndex())
            break;
        if(pset[i].getIndex() == first_packet->getIndex())
            seen_first = true;
        if(!seen_first)
            continue;

        //Packet contributes to goodput if it was not retransmitted
        if(!pset[i].getIsLost()){
            byte_count += pset[i].getDataLength();
        }
    }

    int time_us = last_packet->getTimestampUs() - first_packet->getTimestampUs();
    return byte_count * 8 * pow(10,6) / time_us;
 }

float mean(vector<int> p)
{
	float average = 0.0;
	for(int  k =0;k <p.size();k++)
	{
		average+=p[0];
	}
	average/=p.size();
	return average;
}

float median(vector<int> p)
{
    // First we sort the array 
    vector<int> q;
    for(int i =0;i<p.size();i++)
    {
    	q.push_back(p[i]);
    }
    sort(q.begin(), q.end()); 
  
    // check for even case 
    if (q.size() % 2 != 0) 
        return (float)q[q.size() / 2]; 
  
    return (float)(q[(q.size() - 1) / 2] + q[q.size() / 2]) / 2.0; 
} 
  


int percentile(vector<int> s, int n)
{
	vector<int> j;
	for(int k = 0;k < s.size();k++)
	{
		j.push_back(s[k]);
	}
	sort(j.begin(), j.end()); 
	int rank = n/100*(s.size());
	return j[rank-1];
}

PolicingParams getPolicingParamsForEndpoint(Endpoint e,int cutoff)
{
	
	 //Detect first and last loss
	
	vector<Packet> pset = e.getPackets();
	Packet *first_loss = NULL,*first_loss_no_skip = NULL,*last_loss = NULL;
	int skipped  = 0;
	for(int i = 0 ;i < e.getPackets().size();i++)
	{
		if(pset[i].getIsLost())
		{
             if(first_loss_no_skip == NULL)
             	first_loss_no_skip = &pset[i];
             else if(cutoff == skipped){
             	first_loss = &pset[i];
                break;
            }
             else 
             	skipped++;

		}
	}

	if(first_loss == NULL){
		PolicingParams p1(RESULT_INSUFFICIENT_LOSS);
        return p1;
    }


    for(int j = pset.size()-1;j >=0;j--)
    {

		if(pset[j].getIsLost())
		{
             if(cutoff == skipped){
             	last_loss = &pset[j];
                break;
            }
             else 
             	skipped++;

		}
    }

    if(last_loss == NULL){
		PolicingParams p2(RESULT_INSUFFICIENT_LOSS);
        return p2;
    }

    
   /* 2.Late loss threshold exceed scenario*/
    
    if(first_loss->getSeqRelative() > LATE_LOSS_THRESHOLD){
        PolicingParams p3(RESULT_LATE_LOSS);
        return p3;
     }

    
   /* Check the GOODPUT*/
    
     int policingRate = goodPutBetweenFirstAndLastLoss(e,first_loss,last_loss);
     cout<<"policing rate: "<<policingRate<<endl;

    int median_rtt_us = e.getMedianRtt() * 1000;
    int loss_zero_threshold = ZERO_THRESHOLD_LOSS_RTT_MULTIPLIER * median_rtt_us * policingRate / (8*pow(10,6));
    int pass_zero_threshold = ZERO_THRESHOLD_PASS_RTT_MULTIPLIER * 
        median_rtt_us * policingRate / (8* pow(10,.6));
    int y_intercept = first_loss->getSeqRelative() - (policingRate * (
        first_loss->getTimestampUs() - (e.getPackets())[0].getTimestampUs()) / (8 * pow(10,6)));
    if(y_intercept < -1 * pass_zero_threshold){
        PolicingParams p3(RESULT_NEGATIVE_FILL);
        return p3;
    }

    int tokens_available = 0;
    int tokens_used = 0;
    vector<int> tokens_on_loss;
    vector<int> tokens_on_pass;

    bool seen_first = false,seen_first_no_skip = false;
    int burst_size = 0;
    int inflated_rtt_count = 0;
    int all_rtt_count = 0;
    vector<int> rtts;

    int slices_with_loss = 1;
    int slice_end = first_loss->getTimestampUs() + median_rtt_us;

    int ignore_index = -1;

   for(int i = 0;i < pset.size();i++){
        /*We only consider ACK delay values for our RTT distribution if there are no pending losses
          that might result in a out-of-order reception delay*/
        if(pset[i].getHasRtx() != false)
            ignore_index = max(ignore_index, pset[i].getAckIndex());
        if(pset[i].getHasRtx() == false && pset[i].getAckDelay() != -1 && pset[i].getIndex()> ignore_index)
            rtts.push_back(pset[i].getAckDelay());

        if(&pset[i] == first_loss)
            seen_first = true;
        if(&pset[i] == first_loss_no_skip)
            seen_first_no_skip = true;
        if(!seen_first_no_skip)
            burst_size += pset[i].getDataLength();
        if(!seen_first)
            continue;

        float tokens_produced = policingRate * (pset[i].getTimestampUs() - first_loss->getTimestampUs()) / (pow(10,6) * 8);
        float tokens_available = tokens_produced - tokens_used;
        if(pset[i].getIsLost()){
            tokens_on_loss.push_back(tokens_available);
            if (rtts.size() > 1 and rtts[rtts.size()-2] >= percentile(rtts, 50)
                    && rtts[rtts.size()-2] > INFLATED_RTT_THRESHOLD * percentile(rtts, INFLATED_RTT_PERCENTILE)
                    && rtts[rtts.size()-2] >= 20){
                inflated_rtt_count += 1;
            }
            all_rtt_count += 1;
            if(pset[i].getTimestampUs() > slice_end){
                slice_end = pset[i].getTimestampUs() + median_rtt_us;
                slices_with_loss += 1;
            }
        }
        else {
            tokens_on_pass.push_back(tokens_available);
            tokens_used += pset[i].getDataLength();
        }
     }

    if(slices_with_loss < MIN_NUM_SLICES_WITH_LOSS){
    	PolicingParams p5(RESULT_INSUFFICIENT_LOSS);
        return p5;
    }
    if(tokens_on_loss.size() < MIN_NUM_SAMPLES || tokens_on_pass.size() < MIN_NUM_SAMPLES){
        return PolicingParams(RESULT_INSUFFICIENT_LOSS);
    }

    
    

  
   
     //3.Exceeding policing rate check
    float mean_tokens_on_loss = mean(tokens_on_loss);
    if((mean_tokens_on_loss) >= mean(tokens_on_pass) ||  median(tokens_on_loss) >= median(tokens_on_pass))
        return PolicingParams(RESULT_HIGHER_FILL_ON_LOSS);

    int median_tokens_on_loss = median(tokens_on_loss);
    int out_of_range = 0;
    for(int l = 0;l<tokens_on_loss.size();l++){
        if(abs(tokens_on_loss[l] - median_tokens_on_loss) > loss_zero_threshold)
            out_of_range += 1;
    }
    if(tokens_on_loss.size() * ZERO_THRESHOLD_LOSS_OUT_OF_RANGE < out_of_range){
        return PolicingParams(RESULT_LOSS_FILL_OUT_OF_RANGE);
    }
    /* c. Token bucket is NOT empty when packets go through, i.e.
    #    the number of estimated tokens in the bucket should not be overly negative
    #    To account for possible imprecisions regarding the timestamps when the token bucket
    # was empty, we subtract the median fill level on loss from all token
    # count samples.*/
    out_of_range = 0;
    for(int l = 0;l < tokens_on_pass.size();l++){
        if(tokens_on_pass[l] - median_tokens_on_loss < -pass_zero_threshold)
            out_of_range += 1;
    }
    if(tokens_on_pass.size() * ZERO_THRESHOLD_PASS_OUT_OF_RANGE < out_of_range)
        return PolicingParams(RESULT_PASS_FILL_OUT_OF_RANGE);

    // d. RTT did not inflate before loss events
    float rtt_threshold = INFLATED_RTT_TOLERANCE * all_rtt_count;
    // print "threshold: %d, count: %d" % (rtt_threshold, inflated_rtt_count)
    /*if(inflated_rtt_count > rtt_threshold)
        return PolicingParams(RESULT_INFLATED_RTT);*/


    PolicingParams policed(RESULT_POLICED, policingRate, burst_size);
    return policed;
}

void processFile()
{
	std::ifstream t;
//std::string line;
vector<Flow> f;
vector<Packet> packet_list;
vector<Endpoint> endpoint_list;
string line;
int nest = -1;
t.open("datajs.json");
std::getline(t, line);
while(t){
if(line.find("[") != string::npos){
   nest++;
}
else if(line.find("]") != string::npos){
	nest--;
	if(nest == 0){
     Flow *f1 = new Flow();
     f1->setEndpointList(endpoint_list);
     f.push_back(*f1);
     endpoint_list = vector<Endpoint>();
    }
    else if(nest == 1){
       Endpoint *e = new Endpoint();
       e->setPacketList(packet_list);
       endpoint_list.push_back(*e);
    }
	packet_list = vector<Packet>();
}
else if(line.find('{') != string::npos || line.find('}') != string::npos)
{
	
}
else if(line.find("[]") != string::npos )
 cout<<"empty";

else {
	Packet *p = new Packet();
	while(t && line.find(":") != string::npos){
    
	int pos = line.find(":");
	string value = line.substr(pos+1);
    string name = line.substr(0,pos);
    if(name.find("is_lost") != string::npos){
    	if(value.find("false") != string::npos)
          p->setIsLost(false);
        else {
          p->setIsLost(true);
        }
    }
    else if(name.find("timestamp_us") != string::npos)
    	 p->setTimestampUs(stol(value));
    else if(name.find("ack_delay_ms") != string::npos)
    	 p->setAckDelay(stof(value));
    else if(name.find("index") != string::npos)
    	 p->setIndex(stoi(value));
    else if(name.find("seq_relative") != string::npos)
    	 p->setSeqRelative(stol(value));
    else if(name.find("data_length") != string::npos)
    	 p->setDataLength(stoi(value));
    	
    std::getline(t, line);
   }
   packet_list.push_back(*p);
}

std::getline(t, line);
// ... Append line to buffer and go on
}
int sum = 0;
vector<Endpoint> endpointList;
std::ifstream t2;
t2.open("data2js.json");
std::getline(t2, line);
for(int k = 0;k < f.size();k++)
{
	vector<Endpoint> ep = f[k].getEndpointList();
	for(auto e : ep)
	{
        if(t2)
        {
        	while(line.find("median") == string::npos)
        	{
                  std::getline(t2, line);
        	}
        	int pos = line.find(":");
        	string value = line.substr(pos+1);
        	e.setMedianRtt(stof(value));
        	std::getline(t2, line);
        }
		getPolicingParamsForEndpoint(e,0);
	}
}
cout<<sum;


//cout<<f[0].getEndpointList().size();
t.close();

}

int main()
{
processFile();
return 0;
}
