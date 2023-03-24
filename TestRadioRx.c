/*
 * File: TestRadioRx
 *
 * Contains: Test radio receiving
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

// In this test we do basic input/output using the radio. There are two build
// configs, one to build a sender (Tx) and one to build a receiver (Rx). For a
// more complex example see: https://docs.koliada.com/kes/examples/TestRadio

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
#define RF_CHANNEL 15
#define RangeResponse 0x02
#define RangeRequest 0x11
#define Initial 0x01
#define RangeCalc 0x12

// local frame buffer
#define kMaxFrameSize 128
static byte rxBuf[kMaxFrameSize];

DefineEvent(rxEvent);
void rxEventHandler(EVENT e, byte *buf, word len)
	{
        static byte txBuf[] = "EW:";
        switch(buf[0]) {
          case Initial:
            print("Received Initial\n");
            txBuf[3] = RangeRequest;
            radioPutch(radio, (byte *)txBuf, 4);
            break;
            
          case RangeResponse: 
            print("Received Range Response\n");
            print("Range:\n");
            int r = rand();
            printf("%d", r);
            print("\n");
            txBuf[3] = RangeCalc;
            
            txBuf[4] = ((byte *)&r)[0];
            txBuf[5] = ((byte *)&r)[1];
            ((byte *)&r)[0] = txBuf[4];
            ((byte *)&r)[1] = txBuf[5];
            print("\n");

            radioPutch(radio, (byte *)txBuf, 10);
            break;

        }
        
        //byte b = buf[0];
        //printMsg(b++);
        
        
        
        //building a reply msg
        //static byte txBuf[] = "EW:";
        //txBuf[3] = b;

        //dump(txBuf, 16, 1);
        radioPutch(radio, (byte *) txBuf, 6);
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
	print("%p[%u]: done!\n", frame, len);
	dump(frame, 16, 1);
	}

 void TEST()
	{
	printf(">>%s\n", __func__);

	// init the radio
	initRadio();

	debug("Setting up to RECIEVE\n\n");

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

	// channel
	radioIocntl(radio, kRfSetChannel, RF_CHANNEL);
	
	// start listening
	rfEnableRx();
	
	WaitEvent(0, 0);

	// we're done
	printf("<<%s\n", __func__);

	// When a KoliadaES program exits, control returns to the kernel and any exit
	// delegates are run.
	//
	// Typically an embedded <application> program does NOT exit, however, some
	// programs are designed to exit. They may be configuration programes, or driver
	// installers, test cases and similar.
	//
	// What happens after exit completes is host/developer defined (as part of board
	// configuration. Typically, OTA will run.

	// For details see: https://docs.koliada.com/kes/ExitHandling
	}

