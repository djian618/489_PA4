/*
 * Copyright (c) 2014, 2015, 2016 University of Michigan, Ann Arbor.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of Michigan, Ann Arbor. The name of the University 
 * may not be used to endorse or promote products derived from this 
 * software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Author: Sugih Jamin (jamin@eecs.umich.edu)
 *
*/
#include <stdio.h>         // fprintf(), perror(), fflush()
#include <stdlib.h>        // atoi(), random()
#include <math.h>          // ceil(), floor()
#include <assert.h>        // assert()
#include <limits.h>        // LONG_MAX, INT_MAX
#include <iostream>
using namespace std;
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>      // socklen_t
#include "wingetopt.h"
#else
#include <string.h>        // memset(), memcmp(), strlen(), strcpy(), memcpy()
#include <unistd.h>        // getopt(), STDIN_FILENO, gethostname()
#include <signal.h>        // signal()
#include <netdb.h>         // gethostbyname(), gethostbyaddr()
#include <netinet/in.h>    // struct in_addr
#include <arpa/inet.h>     // htons(), inet_ntoa()
#include <sys/types.h>     // u_short
#include <sys/socket.h>    // socket API, setsockopt(), getsockname()
#include <sys/ioctl.h>     // ioctl(), FIONBIO
#include <sys/time.h>      // gettimeofday()
#endif
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "ltga.h"
#include "socks.h"
#include "netimg.h"
#include "imgdb.h"

#define USECSPERSEC 1000000

/*
 * imgdb::args: parses command line args.
 * seeds the random number generator.
 *
 * Returns 0 on success or 1 on failure.
 *
 * Nothing else is modified.
*/
int imgdb::
args(int argc, char *argv[])
{
  /*
  char c;
  extern char *optarg;
  */

  if (argc > 1) {
    return (1);
  }
  
  /*
  while ((c = getopt(argc, argv, "")) != EOF) {
    switch (c) {
    default:
      return(1);
      break;
    }
  }
  */

  return (0);
}

/*
 * readimg: load TGA image from file "imgname" to curimg.
 * "imgname" must point to valid memory allocated by caller.
 * Terminate process on encountering any error.
 * Returns NETIMG_FOUND if "imgname" found, else returns NETIMG_NFOUND.
 */
char imgdb::
readimg(char *imgname, int verbose)
{
  string pathname=IMGDB_FOLDER;

  if (!imgname || !imgname[0]) {
    return(NETIMG_ENAME);
  }
  
  curimg.LoadFromFile(pathname+IMGDB_DIRSEP+imgname);

  if (!curimg.IsLoaded()) {
    return(NETIMG_NFOUND);
  }

  if (verbose) {
    cerr << "Image: " << endl;
    cerr << "       Type = " << LImageTypeString[curimg.GetImageType()] 
         << " (" << curimg.GetImageType() << ")" << endl;
    cerr << "      Width = " << curimg.GetImageWidth() << endl;
    cerr << "     Height = " << curimg.GetImageHeight() << endl;
    cerr << "Pixel depth = " << curimg.GetPixelDepth() << endl;
    cerr << "Alpha depth = " << curimg.GetAlphaDepth() << endl;
    cerr << "RL encoding = " << (((int) curimg.GetImageType()) > 8) << endl;
    /* use curimg.GetPixels()  to obtain the pixel array */
  }
  
  return(NETIMG_FOUND);
}

/*
 * marshall_imsg: Initialize *imsg with image's specifics.
 * Upon return, the *imsg fields are in host-byte order.
 * Return value is the size of the image in bytes.
 *
 * Terminate process on encountering any error.
 */
double imgdb::
marshall_imsg(imsg_t *imsg)
{
  imsg->im_depth = (unsigned char)(curimg.GetPixelDepth()/8);
  if (((int) curimg.GetImageType()) == 3 ||
      ((int) curimg.GetImageType()) == 11) {
    imsg->im_format = ((int) curimg.GetAlphaDepth()) ?
      NETIMG_GSA : NETIMG_GS;
  } else {
    imsg->im_format = ((int) curimg.GetAlphaDepth()) ?
      NETIMG_RGBA : NETIMG_RGB;
  }
  imsg->im_width = curimg.GetImageWidth();
  imsg->im_height = curimg.GetImageHeight();

  return((double) (imsg->im_width*imsg->im_height*imsg->im_depth));
}

/* 
 * recvqry: receives an iqry_t packet and stores the client's address
 * and port number in the imgdb::client member variable.  Checks that
 * the incoming iqry_t packet is of version NETIMG_VERS and of type
 * NETIMG_SYNQRY.
 *
 * If error encountered when receiving packet or if packet is of the
 * wrong version or type returns appropriate NETIMG error code.
 * Otherwise returns 0.
 *
 * Nothing else is modified.
*/
char FIFO::
recvqry(iqry_t *iqry)
{
  int bytes;  // stores the return value of recvfrom()

  /*
   * Call recvfrom() to receive the iqry_t packet from
   * client.  Store the client's address and port number in the
   * imgdb::client member variable and store the return value of
   * recvfrom() in local variable "bytes".
  */
  socklen_t len;
  
  len = sizeof(struct sockaddr_in);
  bytes = recvfrom(sd, iqry, sizeof(iqry_t), 0,
                   (struct sockaddr *) &client, &len);
  net_assert((bytes <= 0), "imgdb::recvqry: recvfrom");
  
  if (bytes != sizeof(iqry_t)) {
    return (NETIMG_ESIZE);
  }
  if (iqry->iq_vers != NETIMG_VERS) {
    return(NETIMG_EVERS);
  }
  if (iqry->iq_type != NETIMG_SYNQRY) {
    return(NETIMG_ETYPE);
  }
  if (strlen((char *) iqry->iq_name) >= NETIMG_MAXFNAME) {
    return(NETIMG_ENAME);
  }

  return(0);
}
  
/* 
 * sendpkt: sends the provided "pkt" of size "size"
 * to imgdb::client using sendto().
 *
 * Returns the return value of sendto().
 *
 * Nothing else is modified.
*/
int imgdb::
sendpkt(char *pkt, int size)
{
  int bytes;
    
  bytes = sendto(sd, pkt, size, 0, (struct sockaddr *) &client,
                 sizeof(struct sockaddr_in));
  if (bytes == size) {
    return(0);
  }

  return(-1);
}

/* 
 * sendimsg: send the specifics of the image (width, height, etc.) in
 * an imsg_t packet to the client.  The data type imsg_t is defined in
 * netimg.h.  Assume that other than im_vers, the other fields of
 * imsg_t are already correctly filled, but integers are still in host
 * byte order.  Fill in im_vers and convert integers to network byte
 * order before transmission.  The field im_type is set by the caller
 * and should not be modified.  Call imgdb::sendpkt() to send the imsg
 * packet to the client.
 *
 * Return what sendpkt() returns.
 * Nothing else is modified.
*/
int imgdb::
sendimsg(imsg_t *imsg)
{
  imsg->im_vers = NETIMG_VERS;
  imsg->im_width = htons(imsg->im_width);
  imsg->im_height = htons(imsg->im_height);

  return(sendpkt((char *) imsg, sizeof(imsg_t)));
}

/*
 * sendimg:
 * Send the image contained in *image to the client.
 * Send the image in chunks of segsize, not to exceed mss,
 * instead of as one single image. Each segment can be sent only if
 * there's enough tokens to cover it.  The token bucket filter
 * parameters are "imgdb::bsize" tokens and "imgdb::trate" tokens/sec.
 *
 * Terminate process upon encountering any error.
 * Doesn't otherwise modify anything.
*/
void imgdb::
sendimg(char *image, long imgsize)
{
  int bytes, segsize, datasize;
  char *ip;
  long left;
  unsigned int snd_next=0;
  int err, offered, usable;
  socklen_t optlen;
  struct msghdr msg;
  struct iovec iov[NETIMG_NUMIOV];
  ihdr_t hdr;

  if (!image) {
    return;
  }
  
  ip = image; /* ip points to the start of image byte buffer */
  datasize = mss - sizeof(ihdr_t) - NETIMG_UDPIP;
  
  /* Lab7 Task 2:
   * Initialize your token bucket filter variables, if any.
   */
  /* Lab7 YOUR CODE HERE */
  float current_bsize =  bsize;
  /* 
   * make sure that the send buffer is of size at least mss.
   */
  offered = mss;
  optlen = sizeof(int);
  err = getsockopt(sd, SOL_SOCKET, SO_SNDBUF, &usable, &optlen);
  if (usable < offered) {
    err = setsockopt(sd, SOL_SOCKET, SO_SNDBUF, &offered,
                     sizeof(int));
    net_assert((err < 0), "imgdb::sendimg: setsockopt SNDBUF");
  }
  
  /* 
   * Populate a struct msghdr with information of the destination
   * client, a pointer to a struct iovec array.  The iovec array
   * should be of size NETIMG_NUMIOV.  The first entry of the iovec
   * should be initialized to point to an ihdr_t, which should be
   * re-used for each chunk of data to be sent.
   */
  msg.msg_name = &client;
  msg.msg_namelen = sizeof(sockaddr_in);
  msg.msg_iov = iov;
  msg.msg_iovlen = NETIMG_NUMIOV;
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;
  
  hdr.ih_vers = NETIMG_VERS;
  hdr.ih_type = NETIMG_DATA;
  iov[0].iov_base = &hdr;
  iov[0].iov_len = sizeof(ihdr_t);
  
  do {
    /* size of this segment */
    left = imgsize - snd_next;
    segsize = datasize > left ? left : datasize;
    
    /*
     * Lab7 Task 2:
     *
     * If there isn't enough token left to send one segment of
     * data, usleep() until *at least* as many tokens as needed to send a
     * segment, plus a random multiple of bsize amount of data would
     * have been generated.  To determine the "random multiple" you may
     * use the code ((float) random()/INT_MAX) to generate a random number
     * in the range [0.0, 1.0].  Your token generation rate is a given by
     * the "trate" formal argument of this function.  The unit of "trate"
     * is tokens/sec.
     *
     * Enforce that your token bucket size is not larger than "bsize"
     * tokens.  The variable "bsize" is a formal argument of this
     * function.
     *
     * Also decrement token bucket size when a segment is sent.
     */
    /* Lab7 YOUR CODE HERE */
     float token_need = ((float)(segsize/IMGDB_BPTOK));
     if(current_bsize<token_need){
        float token_need_to_generate = token_need-current_bsize+((float)random()/INT_MAX)*bsize;
        float time_wait = ((float)((token_need_to_generate)/trate));
        int judge = usleep(time_wait*1000000);
        current_bsize = min(bsize,token_need_to_generate+current_bsize);
        net_assert(judge > 0,"usleep work not in full function")
     }
     current_bsize -= token_need;
    /* 
     * With sufficient tokens in the token bucket, send one segment
     * of data of size segsize at each iteration.  Point the second
     * entry of the iovec to the correct offset from the start of
     * the image.  Update the sequence number and size fields of the
     * ihdr_t header to reflect the byte offset and size of the
     * current chunk of data.  Send the segment off by calling
     * sendmsg().
     */
    iov[1].iov_base = ip+snd_next;
    iov[1].iov_len = segsize;
    hdr.ih_seqn = htonl(snd_next);
    hdr.ih_size = htons(segsize);
    
    bytes = sendmsg(sd, &msg, 0);
    net_assert((bytes < 0), "imgdb::sendimg: sendmsg");
    net_assert((bytes != (int)(segsize+sizeof(ihdr_t))),
               "imgdb::sendimg: sendmsg bytes");
    
    fprintf(stderr, "imgdb::sendimg: sent offset 0x%x, %d bytes\n",
            snd_next, segsize);
    
    snd_next += segsize;
  } while ((int) snd_next < imgsize);

  return;
}


/*FIFO::nextxmission() comprises the Task 2 portion of
imgdb::sendimg() from Lab 7. It may be
convenient to call Flow::nextFi() from 
FIFO::nextxmission() to compute the current segment size 
even if we don't need to compute any finish time.*/ 

float FIFO::nextmission(){

  //time need if there are enough tokens  this is second 
  float fifo_next_time = running_flow.nextFi(1);
  float token_need = ((float)(running_flow.segsize/IMGDB_BPTOK));

     if(current_bsize < token_need){
        float token_need_to_generate = token_need-current_bsize+((float)random()/INT_MAX)*bsize;
        float time_wait = ((float)((token_need_to_generate)/trate));
                
        current_bsize = min(bsize,token_need_to_generate+current_bsize);
        net_assert(judge > 0,"usleep work not in full function")
        return (time_wait*1000000)+(fifo_next_time*1000000);
     }
     current_bsize -= token_need;








}


/*
 * handleqry: accept connection, then receive a query packet, search
 * for the queried image, and reply to client.
 */
void FIFO::
handleqry()
{
  
  iqry_t iqry;
  imsg_t imsg;
  double imgdsize;
  struct timeval start, end;
  int usecs, secs;
  // The FIFO query handler should check 
  //whether there's already an active flow in 
  //the FIFO queue. If so, return an imsg_t packet
  // with im_type set to NETIMG_EFULL to the client

  imsg.im_type = recvqry(&iqry);
  if(running_flow.in_use == 1){
    imsg.im_type = NETIMG_EFULL;
    sendimsg(&imsg);
    return;    
  }


  if (imsg.im_type) { // error
    fprintf(stderr, "imgdb::handleqry: recvqry returns 0x%x.\n", imsg.im_type);
    sendimsg(&imsg);
  } else {
    
    imsg.im_type = readimg(iqry.iq_name, 1);
    
    if (imsg.im_type == NETIMG_FOUND) {
      mss = (unsigned short) ntohs(iqry.iq_mss);
      rwnd = iqry.iq_rwnd;
      //modify in the imgdb recive part
      frate = (unsigned short) iqry.iq_frate;
      net_assert(!(mss && rwnd && frate),
                 "imgdb::sendimg: mss, rwnd, and frate cannot be zero");
      /*
       * Lab7 Task 1:
       * 
       * Compute the token bucket size such that it is at least big enough
       * to hold as many tokens as needed to send one "mss" (excluding
       * headers).  If "rwnd" is greater than 1, token bucket size should be
       * set to accommodate as many segments (excluding headers).
       * Store the token bucket size in imgdb::bsize.  The unit
       * of bsize is "number of tokens".
       *
       * Compute the token generation rate based on target flow send rate,
       * stored in imgdb::frate.  The variable "frate" is
       * in Kbps.  You need to convert "frate" to the appropriate token
       * generation rate, in unit of tokens/sec.  Store the token
       * generation rate in imgdb::trate.
       *
       * Each token covers IMGDB_BPTOK bytes, defined in imgdb.h.
       */
      /* Lab7: YOUR CODE HERE */
      bsize = ceil((float)((mss-sizeof(ihdr_t)-NETIMG_UDPIP)*rwnd/IMGDB_BPTOK));
      trate = (frate*1000)/(IMGDB_BPTOK*8); 
      running_flow.init(sd, &client, &iqry, &imsg, currFi); 
      current_bsize =  bsize;
    } else {
      sendimsg(&imsg);
    }
  }

  return;
}

int
main(int argc, char *argv[])
{ 
  socks_init();

  imgdb imgdb;
  
  // parse args, see the comments for imgdb::args()
  if (imgdb.args(argc, argv)) {
    fprintf(stderr, "Usage: %s\n", argv[0]); 
    exit(1);
  }

  while (1) {
    imgdb.handleqry();
  }
    
#ifdef _WIN32
  WSACleanup();
#endif // _WIN32
  
  exit(0);
}
