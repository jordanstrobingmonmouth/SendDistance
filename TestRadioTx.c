/*
 * File: TestRadioTx.c
 *
 * Contains: Test radio Tx
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 *  Copyright:	© 1989-2020 by Guy McIlroy
 *  All rights reserved.
 *
 *  Use of this code is subject to the terms of the KoliadaESDK license.
 *  https://docs.koliada.com/KoliadaESDKLicense.pdf
 *
 *  Koliada, LLC - www.koliada.com
 *  Support: forum.koliada.com
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 */
#include "Koliada.h"
#include "messages.h"
// 'radio' is a (single threaded) kernel installed driver.

#define RangeResponse 0x02
#define RangeRequest 0x11
#define Initial 0x01
#define RangeCalc 0x12


#define RF_CHANNEL 15

// In this test we do basic input/output using the radio. There are two build configs,
// one to build a sender and one to build a receiver. For a more complex example
// see: https://docs.koliada.com/kes/examples/TestRadoio

////////////////////////////////////////////////////////////////////////////////
//
// For details see: https://docs.koliada.com/kes/examples/TestRadoioApi
//
////////////////////////////////////////////////////////////////////////////////


// frame 'data layer' ID
#if 1
#define FRAME_ID0 'E'
#define FRAME_ID1 'W'
#else
#define FRAME_ID0 'M'
#define FRAME_ID1 '0'
#endif

// local frame buffer
#define kMaxFrameSize 128
static byte rxBuf[kMaxFrameSize];

static byte msg = 0;
DefineEvent(rxEvent);
void rxEventHandler(EVENT e, byte *buf, word len)
	{
	// running in application context
	// print("%s\n", (char *)buf);
        // dump((char *)buf,len,1);
        // print(buf[0] + buf[1] + buf[3]);
        // printf("%u\n %u\n", (byte) buf[0], (byte) buf[1]);
        // print("%s\n", (char *)buf[0]);
        //char * buffer = (char *) buf;
        //print("%s\n", buffer);
        //char subbuff[5];
        //memcpy( subbuff, &buffer[14], 4 );
        //subbuff[4] = '\0';
        //byte b = (byte) strtol(subbuff, (char **)NULL, 16);
        //printMsg(b++);
        
        //building a reply msg
        //dump(buf, 16, 1);
        static byte txBuf[] = "EW:";
        switch(buf[0]) {
          case RangeRequest:
            print("Received Range Request\n");
            txBuf[3] = RangeResponse;
            radioPutch(radio, (byte *)txBuf, 4);
            break;
          case RangeCalc:
            print("Received Range Calculation\n");
            int x = buf[1] | buf[2] << 8;
            printf("%d", x);
            print("\n");
            break;

        }

	}

StaticDelegate(rxReady);
void rxReadyHandler(byte *buf, word len)
	{
	// running in the interrupt handler!!
	// called each time the a frame is recieved
	// buf[0] = RSSI
	// buf[1] = CORR
	// buf[2] = protocol type

	// At this point the frame is qualified only as having a frame signature
	// that matches the frame signature criteria defined by the kRfSetFrameSig
	// call made to establish the frame signature filter (see below in TEST())
	//
	// This callback is so the frame can be qualified in relation protocol parsing
	// and then forwarded (and possibly queued) to the application for further
	// processing.
	//
	// Since this is running on the interrupt handler we don't want to waste time!
	//
	// For this test we have a very simple 'data layer' protocol and once qualified
	// we simply pass the message on to the application as an event. For simple the
	// processing required here, an event is overkill, but more complex protocols
	// _must_ queue frames and defer processing to the application to facilitate
	// radio throughput.
	//
	// Further note, this system is event driven and without the posting of an
	// event, the _application_ will never know that anything changed!

	// qualify and pass up to the application
	switch (buf[2])
		{
		// post the rxEvent
		case ':':// protocol id
			// skip the protocol headers
			PostEvent(rxEvent, &rxBuf[3], len - 3);
			break;

		// default is ignored
		default:
			break;
		}

	// Here, we have an overly simple protocol and do no buffering or queuing - any
	// new incoming frame will overwrite the rxBuf before or during the handling
	// of it by the application. A 'real' application will have buffering and/or
	// queuing and buffer/queue protection will be implicitly defined by the protocol.
	//
	// For an additional example see: https://docs.koliada.com/kes/examples/TestRadio
	}

StaticDelegate(txDoneDelegate);
void txDoneHandler(byte *frame, word len)
	{
	// running in the interrupt handler!!
	// called each time the a frame is sent
	//print("%p[%u]: done!\n", frame, len);
	//dump(frame, 16, 1);
	}

 void TEST()
	{
	printf(">>%s\n", __func__);

	// init the radio
	initRadio();

	debug("Setting up to TRANSMIT\n\n");
        

	// set the txDone handler
	// set up the txDone delegate (shows we're transmitting)
	objectCreate(txDoneDelegate, delegateTask(txDoneHandler));
	rfIocntl(kRfAddTxDone, txDoneDelegate);

	// specifically not using CCA in this test
	// CCA requires that the receiver is on and this test requires that receiver remains off

	radioIocntl(radio, kRfSetChannel, RF_CHANNEL);

	#define kTxBufSize 125
	int maxFrameSize = radioIocntl(radio, kRfGetMaxFrameSize);

	assert(maxFrameSize <= kTxBufSize);
	print("Max frame size = %u\n", maxFrameSize);
        
        // create the rxEvent
	objectCreate(rxEvent);
	OnEvent(rxEvent, (HANDLER) rxEventHandler);

	// set the rxReady handler
	// create & set the Rx delegate
	objectCreate(rxReady, delegateTask(rxReadyHandler));
	radioIocntl(radio, kRfAddRxReady, rxReady);

	// set the buffer,
	radioIocntl(radio, kRfSetRxBuf, rxBuf, sizeof(rxBuf));

	// frame type, and
	rfIocntl(kRfSetFrameSig, FRAME_ID0 << 8 | FRAME_ID1);
	
	UInt16 sig;
	rfIocntl(kRfGetFrameSig, &sig);
	
	// start listening
	rfEnableRx();

        

	print("hit any key to send\nhit ESC to quit\n");
	do
		{
		int key; while ((key = getch()) == -1) EventYield();	// get a (uart) keypress (no wait)
		if (key == 0x1b)
			// ESC
			break;
        
                static byte txBuf[] = "EW:";
                txBuf[3] = 0x01;
                radioPutch(radio, (byte *)txBuf, 6);
		} while (1);


	// we're done
	printf("<<%s\n", __func__);

	// When a KoliadaES program exits, control returns to the kernel and any exit
	// delegates are run. using(radio) (or initRadio) installed an exit delegate,
	// which releases the radio as part of exit processing.
	//
	// Typically an embedded <application> program does NOT exit, however, some
	// programs are designed to exit. They may be configuration programes, or driver
	// installers, test cases and similar.
	//
	// What happens after exit completes is host/developer defined (as part of board
	// configuration. Typically, OTA will run.

	// For details see: https://docs.koliada.com/kes/ExitHandling
	}
