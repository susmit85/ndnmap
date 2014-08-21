/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014, Washington University in St. Louis,
 *
 */

#include <boost/asio.hpp>
#include <ndn-cxx/face.hpp>
//#include <ndn-cxx/management/nfd-controller.hpp>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <expat.h>

#define NUM_RECORDED_INTERESTS 2048
#define DEBUG 0
#define MON_NAME_PREFIX "ndn:/ndn/edu/wustl/ndnstatus"

void start_element(void *data, const char *el, const char **attr);
void end_element(void *data, const char *el);
void char_data_handler(void* data, const char* s, int len);


// global variables to support xml parsing
int parser_on = 0;
char current_element[256];
char current_time[50];

struct face_status
{
  char ipaddr[50];
  char tx[50];
  char rx[50];
  int id;
  int valid;
};

struct face_status* current_face;

class NdnmapClient
{
public:
  
  struct interestData {
    bool valid;
    char ipaddr[50];
    unsigned long tx;
    unsigned long rx;
    char current_time[50];
  };
  
  NdnmapClient(char* programName)
  : m_programName(programName)
  , m_prefixName("ndn:/ndn/edu/wustl/ndnstatus")
  {
    m_myipaddr.clear();
    m_pollPeriod = 1;
    recordedInterestIndex = 0;

  }
  
  void
  usage()
  {
    std::cout << "\n Usage:\n " << m_programName << " [poll period] \n"
    ""
    " pull local nfd performace and send it as a prefix to a remote server.\n"
    "\n"
    " \t-h - print this message and exit\n"
    " \t-i <ipaddr> - host ip address (of this host) to be included in interests\n"
    "\n";
    exit(1);
  }
  
  void
  onData(const ndn::Interest& interest, ndn::Data& data)
  {
    std::cout << "onData" << std::endl;
  }
  
  void
  onTimeout(const ndn::Interest& interest)
  {
    std::cout << "onTimeout" << std::endl;
  }
  
  void
  initInterests()
  {
    int i;
    for (i=0; i < NUM_RECORDED_INTERESTS; i++)
      recordedInterestData[i].valid = false;
    recordedInterestIndex = 0;
  }
  void
  recordInterest(char *ipaddr, char *rx, char *tx, char *time)
  {
    int i;
    
    for (i=0; i < recordedInterestIndex; i++)
    {
      if(current_face->valid != 1)
        continue;
      
      if (recordedInterestData[i].valid && !strcmp(recordedInterestData[i].ipaddr, ipaddr))
      {
        // Found it!
        //printf("recordInterest: Found an already recorded ipaddr: %s\n", ipaddr);
        recordedInterestData[i].tx += atol(tx);
        recordedInterestData[i].rx += atol(rx);
        strcpy(recordedInterestData[i].current_time, time);
        return;
      }
    }
    
    if (i < NUM_RECORDED_INTERESTS)
    {
      if (current_face->valid == 1)
      {
        recordedInterestData[i].valid = true;
        recordedInterestData[i].tx = atol(tx);
        recordedInterestData[i].rx = atol(rx);
        strcpy(recordedInterestData[i].current_time, time);
        strcpy(recordedInterestData[i].ipaddr, ipaddr);
        recordedInterestIndex++;
      }
    }
    else
    {
      printf("recordInterest: TOO MANY FACES!!!\n");
    }
    
    
  }
  void
  printInterests()
  {
    int i;
    for (i=0; i < recordedInterestIndex; i++) {
      if (recordedInterestData[i].valid == false)
        return;
      else {
        printf("interest[%i]: %s %ld %ld %s\n", i, recordedInterestData[i].ipaddr ,recordedInterestData[i].tx ,recordedInterestData[i].rx ,recordedInterestData[i].current_time);
      }
    }
  }

  // This function send http request to the local ndnd.
  // char * response - stores http response
  int SendHTTPGetRequest(char * response)
  {
    int port = 8080;//atoi(NDN_DEFAULT_UNICAST_PORT);//9695;
    const char *host = "localhost";
    struct hostent *server;
    struct sockaddr_in serveraddr;
    char request[1000];
    int n = 1, total = 0, found = 0;
    
    int tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
    
    if (tcpSocket < 0)
    {
      printf("\nError opening socket\n");
      return 0;
    }
    else
    {
      if (DEBUG)
        printf("\nSuccessfully opened socket\n");
    }
    server = gethostbyname(host);
    if (server == NULL)
    {
      printf("gethostbyname() failed\n");
      return 0;
    }
    
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    
    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    
    serveraddr.sin_port = htons(port);
    
    if (connect(tcpSocket, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
    {
      if (DEBUG)
        printf("Error Connecting\n");
    }
    else
    {
      if (DEBUG)
        printf("Successfully Connected\n");
    }
    bzero(request, 1000);
    
    sprintf(request, "GET /?f=xml HTTP/1.1\r\n"
            "Accept: */*\r\n"
            "\r\n");
    
    
    if (send(tcpSocket, request, strlen(request), 0) < 0)
      printf("Error with send()\n");
    else
    {
      if (DEBUG)
        printf("Successfully sent html fetch request\n");
    }
    
    bzero(request, 1000);
    
    
    // Keep reading up to a '</nfdStatus>'
    while (!found)
    {
      n = recv(tcpSocket, response + total, 1024, 0);
      
      if (n == -1)
      {
        // Error, check 'errno' for more details
        break;
      }
      total += n;
      response[total] = '\0';
      
      found = (strstr(response, "</nfdStatus>") != 0);
    }
    
    close(tcpSocket);
    return 1;
  }
  void
  sendRecordedInterests()
  {
    char tmp_name[512];
    int i;
    
    for (i=0; i < recordedInterestIndex; i++)
    {
      if (recordedInterestData[i].valid == false)
        return;
     
      if (DEBUG)
        printf("sendRecordedInterests(): interest[%i]: %s %ld %ld %s\n", i, recordedInterestData[i].ipaddr ,
                                                                          recordedInterestData[i].tx ,
                                                                          recordedInterestData[i].rx ,
                                                                          recordedInterestData[i].current_time);
      
      sprintf(tmp_name, "%s/%s/%s/%s/%ld/%ld", MON_NAME_PREFIX, m_myipaddr.c_str(),
              recordedInterestData[i].ipaddr,
              recordedInterestData[i].current_time,
              recordedInterestData[i].tx,
              recordedInterestData[i].rx);
      
      if (DEBUG)
        printf("sendRecordedInterests(): tmp_name: %s\n", tmp_name);
      
      ndn::Interest i(tmp_name);
      i.setInterestLifetime(ndn::time::milliseconds(1000));
      i.setMustBeFresh(true);
      m_face.expressInterest(i,
                             bind(&NdnmapClient::onData, this, _1, _2),
                             bind(&NdnmapClient::onTimeout, this, _1));
      m_face.processEvents();
      
    }
  }
  void
  run()
  {
    try
    {
      std::cout << m_programName <<  "polling every " << m_pollPeriod << " seconds" << std::endl;
      std::cout << "My IP is: " << m_myipaddr << std::endl;
      
      char *xml_response;
      XML_Parser parser = NULL;
      int offset = 0;
      int skiplines = 0;
      int res = 0;
      
      current_element[0] = '\0';
      current_face = NULL;
      parser_on = 0;
      
      unsigned int size = 1024*1024;
      
      xml_response = (char*)malloc(sizeof(*xml_response)*size);
      
      while (res >= 0 )
      {
        initInterests();
        parser = XML_ParserCreate(NULL);
        if (!parser)
        {
          std::cerr << "Couldn't allocate memory for parser" << std::endl;
          break;
        }
        XML_SetElementHandler(parser, start_element, end_element);
        XML_SetCharacterDataHandler(parser, char_data_handler);
        
        res = SendHTTPGetRequest(xml_response);
        if (res < 1)
        {
          perror("XML get failed\n");
          break;
        }
        if (DEBUG)
          printf("finished read %s\n",xml_response);
        offset = 0;
        skiplines = 0;
        while(skiplines < 5)
        {
          if (xml_response[offset] == '\n')
          ++skiplines;
          ++offset;
        }
        res = XML_Parse(parser, xml_response+offset, strlen(xml_response+offset), 1);
        if (res == 0)
        {
          perror("\n ***** \n XML parse error\n\n");
        }
        else
          res = 1;
        if (current_face != NULL)
        {
          free(current_face);
          current_face = NULL;
        }
        XML_ParserFree(parser);
       
        if (DEBUG)
        {
          printf("ABOUT TO PRINT FACES LIST\n");
          printInterests();
        }
        sendRecordedInterests();
        sleep(m_pollPeriod);
      }
      free(xml_response);
      printf("exit client...\n");
      
      XML_ParserFree(parser);
    }
    catch (std::exception& e)
    {
      std::cerr << "ERROR: " << e.what() << "\n" << std::endl;
      exit(1);
    }
  }
  
  std::string&
  getProgramName()
  {
    return m_programName;
  }
  void
  setPollPeriod(int period)
  {
    m_pollPeriod = period;
  }
  
  int
  getPollPeriod()
  {
    return m_pollPeriod;
  }
  
  void
  setMyIP(char* myipaddr)
  {
    if(DEBUG)
      std::cout << "About to set IP to" << myipaddr << std::endl;
    
    m_myipaddr = myipaddr;
  }
public:
  std::string m_myipaddr;
  
private:
  std::string m_programName;
  std::string m_prefixName;
  int m_pollPeriod;
  ndn::Face m_face;
  
  struct interestData recordedInterestData[NUM_RECORDED_INTERESTS];
  int recordedInterestIndex;
};
NdnmapClient ndnmapClient("ndnxmlstat_c");

void
start_element(void *data, const char *el, const char **attr)
{
  if (strcmp(el, "nfdStatus") == 0)
    parser_on = 1;
  if (!parser_on)
    return;
  strcpy(current_element, el);
}  /* End of start handler */

void
end_element(void *data, const char *el)
{
  if (strcmp(el, "face") == 0 || strcmp(el, "nfdStatus") == 0)
  {
    /*send interest with current_face*/
    if (current_face != NULL)
    {
      //SHOULD STORE DATA HERE:
      ndnmapClient.recordInterest(current_face->ipaddr, current_face->rx, current_face->tx, current_time);
      free(current_face);
      current_face = NULL;
    }
    current_element[0] = '\0';
  }
  else if (strcmp(el, "faces") == 0)
  {
    parser_on = 0;
    if (current_face != NULL)
    {
      free(current_face);
      current_face = NULL;
    }
    current_element[0] = '\0';
  }
}  /* End of end handler */

void
char_data_handler(void* data, const char* s, int len)
{
  if (!parser_on)
    return;
  char tmp_str[256];
  int i = 0;
  int j = 0;
  strncpy(tmp_str, s, len);
  tmp_str[len] = '\0';
  
  if (strcmp(current_element, "faceId") == 0)
  {
    if (current_face != NULL)
    {
      free(current_face);
      printf(" error in parsing received new face before end of old one");
    }
    current_face = (face_status*)malloc(sizeof(*current_face));/*get_face(atoi(tmp_str));*/
    current_face->id = atoi(tmp_str);
    current_face->ipaddr[0] = '\0';
    current_face->tx[0] = '\0';
    current_face->rx[0] = '\0';
    current_face->valid = 0;
  }
  else if (strcmp(current_element, "currentTime") == 0)
  {
    strcpy(current_time,tmp_str);
  }
  else if (strcmp(current_element, "remoteUri") == 0)
  {
    if (current_face != NULL)
    {
      if(strncmp(tmp_str,"udp4",4) == 0 ||
         strncmp(tmp_str,"tcp4",4) == 0)
      {
        current_face->valid = 1;
        j = 7;
      }
      // start copying after ://
      for(i = 0; i < len; ++i)
      {
        if (tmp_str[i+j] == ':')
        {
          current_face->ipaddr[i] = '\0';
          break;
        }
        else
          current_face->ipaddr[i] = tmp_str[i+j];
      }      //strcpy(current_face->ipaddr, tmp_str);
    }
  }
  else if (strcmp(current_element, "incomingBytes") == 0)
  {
    if (current_face != NULL)
      strcpy(current_face->rx,tmp_str);
  }
  else if (strcmp(current_element, "outgoingBytes") == 0)
  {
    if (current_face != NULL)
      strcpy(current_face->tx,tmp_str);
  }
  
}

int
main(int argc, char* argv[])
{
  int option;
  
  
  while ((option = getopt(argc, argv, "hi:")) != -1) {
    switch (option)
    {
      case 'i':
        ndnmapClient.setMyIP(optarg);
        break;
        
      case 'h':
        ndnmapClient.usage();
        break;
        
      default:
        ndnmapClient.usage();
        break;
    }
  }
  
  argc -= optind;
  argv += optind;
  
  if (argv[0] != NULL)
    ndnmapClient.setPollPeriod(atoi(argv[0]));
  
  if(ndnmapClient.m_myipaddr.empty())
  {
    char myHostName[128];
    int res = gethostname(myHostName, 128);
    if (res != 0)
    {
      perror("Could not get my host name");
      exit(1);
    }
    struct hostent *addrResult;
    addrResult = gethostbyname(myHostName);
    if (addrResult == NULL)
    {
      perror("Could not get my host ipaddr: gethostbyname() failed");
      exit(1);
    }
    int i = 0;
    struct in_addr naddr;
    char* tmpStr;
    while(1)
    {
      if(addrResult->h_addr_list[i] == NULL) break;
      bzero((char*)&naddr, sizeof(naddr));
      bcopy((char*)addrResult->h_addr_list[i], (char*)&naddr.s_addr, addrResult->h_length);
      tmpStr = inet_ntoa(naddr);
      
      if (strncmp(tmpStr, "10.", 3) && strncmp(tmpStr, "127.0.", 6) && tmpStr[0] != '0')
      {
        if(DEBUG)
          std::cout << "About to set IP to" << tmpStr << std::endl;
        
        ndnmapClient.m_myipaddr = tmpStr;
        break;
      }
      ++i;
    }
  }
  if(ndnmapClient.m_myipaddr.empty())
  {
    std::cout << "Failed to set local IP adress. Exit." << std::endl;
    exit(0);
  }
  ndnmapClient.run();
  
  return 0;
}
