
/* -------------------------------------------------------------------------
 * Bernese NTRIP Client
 * -------------------------------------------------------------------------
 *
 * Class:      rtigs
 *
 * Purpose:    RTIGS Decoder
 *
 * Author:     L. Mervart
 *
 * Created:    24-Aug-2006
 *
 * Changes:    
 *
 * -----------------------------------------------------------------------*/

#include "rtigs.h"

using namespace std;

#undef L1
#undef L2

// Constructor
////////////////////////////////////////////////////////////////////////////
rtigs::rtigs() : GPSDecoder() {
}

// Destructor
////////////////////////////////////////////////////////////////////////////
rtigs::~rtigs() {
}

// 
////////////////////////////////////////////////////////////////////////////
void rtigs::Decode(char* buffer, int bufLen) {

  // Append the incomming data to the internal buffer
  // ------------------------------------------------
  _buffer.append( QByteArray(buffer, bufLen) );

  // Find the beginning of the message
  // ---------------------------------
  bool found = false;
  for (int ii = 0; ii < _buffer.size(); ii++) { 
    unsigned short xx;
    memcpy( (void*) &xx, &_buffer.data()[ii], sizeof(xx) );
    SwitchBytes( (char*) &xx, sizeof(xx) );
    if (xx == 200) {
      _buffer = _buffer.mid(ii);
      found = true;
      break;
    }
  }
  if (! found) {
    cout << "Message not found\n";
    _buffer.clear();
    return;
  }

  unsigned char* p_buf = (unsigned char*) _buffer.data();  

  unsigned short messType = _GPSTrans.GetRTIGSHdrRecType(p_buf);
  unsigned short numbytes = _GPSTrans.GetRTIGSHdrRecBytes(p_buf);

  // Not enough new data, return
  // ---------------------------
  if (_buffer.size() < numbytes) {
    return;
  }

  // Decode the epoch
  // ----------------
  if (messType == 200) {
    RTIGSO_T  rtigs_obs;
    short numObs = _GPSTrans.Decode_RTIGS_Obs(p_buf, numbytes, rtigs_obs);

////    if (numObs > 0) {
////      _GPSTrans.print_CMEAS();
////    }

    for (short ii = 0; ii < numObs; ii++) {
      Observation* obs = new Observation();

////  obs->statID         =
      obs->SVPRN          = _GPSTrans.DecObs.Obs[ii].sat_prn;
      obs->GPSWeek        = _GPSTrans.DecObs.Obs[ii].GPSTime / 7 / 86400;
      obs->GPSWeeks       = _GPSTrans.DecObs.Obs[ii].GPSTime - 
                                                   obs->GPSWeek * 7 * 86400;
////  obs->sec            = 
      obs->C1             = _GPSTrans.DecObs.Obs[ii].l1_pseudo_range;
////  obs->P1             = _GPSTrans.DecObs.Obs[ii].p1_pseudo_range;
      obs->P2             = _GPSTrans.DecObs.Obs[ii].p2_pseudo_range;
      obs->L1             = _GPSTrans.DecObs.Obs[ii].p1_phase;
      obs->L2             = _GPSTrans.DecObs.Obs[ii].p2_phase;
      obs->SNR1           = (short) _GPSTrans.DecObs.Obs[ii].l1_sn * 10;
      obs->SNR2           = (short) _GPSTrans.DecObs.Obs[ii].l2_sn * 10;
////  obs->pCodeIndicator =
////  obs->cumuLossOfCont =

      m_lObsList.push_back(obs);
    }
  }

  // Unprocessed bytes remain in buffer
  // ----------------------------------
  _buffer = _buffer.mid(numbytes);
}
