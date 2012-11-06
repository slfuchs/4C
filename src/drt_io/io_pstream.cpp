/*----------------------------------------------------------------------*/
/*!
\file io_pstream.cpp

\brief A substitute for STL cout for parallel and complex output schemes.

<pre>
Maintainer: Karl-Robert Wichmann
            wichmann@lnm.mw.tum.de
            http://www.lnm.mw.tum.de
            089 - 289-15237
</pre>
*/

/*----------------------------------------------------------------------*/
/* headers */
#include "io_pstream.H"

namespace IO
{
/// this is the IO::cout that everyone can refer to
Pstream cout;
}


/*----------------------------------------------------------------------*
 * empty constructor                                          wic 11/12 *
 *----------------------------------------------------------------------*/
IO::Pstream::Pstream()
: is_initialized_(false),
  comm_(Teuchos::null),
  targetpid_(-2),
  writetoscreen_(false),
  writetofile_(false),
  outfile_(NULL),
  prefixgroupID_(false),
  groupID_(-2),
  buffer_(std::string())
{}


/*----------------------------------------------------------------------*
 * configure the output                                       wic 11/12 *
 *----------------------------------------------------------------------*/
void IO::Pstream::setup(
  const bool writetoscreen,
  const bool writetofile,
  const bool prefixgroupID,
  Teuchos::RCP<Epetra_Comm> comm,
  const int targetpid,
  const int groupID,
  const std::string fileprefix
)
{
  // make sure that setup is called only once or we get unpredictable behavior
  if (is_initialized_)
    dserror("Thou shalt not call setup on the output twice!");
  is_initialized_ = true;

  comm_          = comm;
  targetpid_     = targetpid;
  writetoscreen_ = writetoscreen;
  writetofile_   = writetofile;
  outfile_       = NULL;
  prefixgroupID_ = prefixgroupID;
  groupID_       = groupID;

  // make sure the target processor exists
  if (targetpid_ >= comm_->NumProc())
    dserror("Chosen target processor does not exist.");

  // prepare the file handle
  if (OnPid() and writetofile_)
  {
    std::stringstream fname;
    fname << fileprefix
          << ".p"
          << std::setfill('0') << std::setw(2) << comm_->MyPID()
          << ".log";
    outfile_ = new std::ofstream(fname.str().c_str());
    if (!outfile_)
      dserror("could not open output file");
  }

  // prepare the very first line of output
  if (OnPid() and prefixgroupID_)
    buffer_ << groupID_ << ": ";
}


/*----------------------------------------------------------------------*
 * close open file handles and reset pstream                  wic 11/12 *
 *----------------------------------------------------------------------*/
void IO::Pstream::close()
{
  if (not is_initialized_)
    return;

  is_initialized_ = false;
  comm_           = Teuchos::null;
  targetpid_      = -2;
  writetoscreen_  = false;
  writetofile_    = false;

  // close file
  if(outfile_)
  {
    outfile_->close();
    delete outfile_;
  }
  outfile_        = NULL;

  prefixgroupID_  = false;
  groupID_        = -2;

  // flush the buffer
  if(writetoscreen_ and OnPid() and buffer_.str().size() > 0)
    std::cout << buffer_.str() << std::flush;
  buffer_.str(std::string());

}


/*----------------------------------------------------------------------*
 * return whether this is a target processor                  wic 11/12 *
 *----------------------------------------------------------------------*/
bool IO::Pstream::OnPid()
{
  if(targetpid_ < 0)
    return true;
  return (comm_->MyPID() == targetpid_);
}


/*----------------------------------------------------------------------*
 * Imitate the std::endl behavior w/out the flush             wic 11/12 *
 *----------------------------------------------------------------------*/
IO::Pstream& IO::endl(IO::Pstream& out)
{
  out << "\n";
  return out;
}


/*----------------------------------------------------------------------*
 * Handle special manipulators                                wic 11/12 *
 *----------------------------------------------------------------------*/
IO::Pstream& operator<<(IO::Pstream& out, IO::Pstream& (*pf)(IO::Pstream&))
{
  return pf(out);
}
