/*****************************************************************************
** FILE IDENTIFICATION
**
**   Name:          phm2pj.cpp
**   Purpose:       Take projections of a phantom object
**   Programmer:    Kevin Rosenberg
**   Date Started:  1984
**
**  This is part of the CTSim program
**  Copyright (C) 1983-2009 Kevin Rosenberg
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License (version 2) as
**  published by the Free Software Foundation.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
******************************************************************************/

#include "ct.h"
#include "timer.h"


enum { O_PHANTOM, O_DESC, O_NRAY, O_ROTANGLE, O_PHMFILE, O_GEOMETRY, O_FOCAL_LENGTH, O_CENTER_DETECTOR_LENGTH,
O_VIEW_RATIO, O_SCAN_RATIO, O_OFFSETVIEW, O_TRACE, O_VERBOSE, O_HELP, O_DEBUG, O_VERSION };

static struct option phm2pj_options[] =
{
  {"phantom", 1, 0, O_PHANTOM},
  {"phmfile", 1, 0, O_PHMFILE},
  {"desc", 1, 0, O_DESC},
  {"nray", 1, 0, O_NRAY},
  {"rotangle", 1, 0, O_ROTANGLE},
  {"geometry", 1, 0, O_GEOMETRY},
  {"focal-length", 1, 0, O_FOCAL_LENGTH},
  {"center-detector-length", 1, 0, O_CENTER_DETECTOR_LENGTH},
  {"offsetview", 1, 0, O_OFFSETVIEW},
  {"view-ratio", 1, 0, O_VIEW_RATIO},
  {"scan-ratio", 1, 0, O_SCAN_RATIO},
  {"trace", 1, 0, O_TRACE},
  {"verbose", 0, 0, O_VERBOSE},
  {"help", 0, 0, O_HELP},
  {"debug", 0, 0, O_DEBUG},
  {"version", 0, 0, O_VERSION},
  {0, 0, 0, 0}
};

static const char* g_szIdStr = "$Id$";


void
phm2pj_usage (const char *program)
{
  std::cout << "usage: " << fileBasename(program) << " outfile ndet nview [--phantom phantom-name] [--phmfile filename] [OPTIONS]\n";
  std::cout << "Calculate (projections) through phantom object, either a predefined --phantom or a --phmfile\n\n";
  std::cout << "     outfile          Name of output file for projections\n";
  std::cout << "     ndet             Number of detectors\n";
  std::cout << "     nview            Number of rotated views\n";
  std::cout << "     --phantom        Phantom to use for projection\n";
  std::cout << "        herman        Herman head phantom\n";
  std::cout << "        shepp-logan   Shepp-Logan head phantom\n";
  std::cout << "        unit-pulse     Unit pulse phantom\n";
  std::cout << "     --phmfile        Get Phantom from phantom file\n";
  std::cout << "     --desc           Description of raysum\n";
  std::cout << "     --nray           Number of rays per detector (default = 1)\n";
  std::cout << "     --rotangle       Angle to rotate view through (fraction of a circle)\n";
  std::cout << "                      (default = select appropriate for geometry)\n";
  std::cout << "     --geometry       Geometry of scanning\n";
  std::cout << "        parallel      Parallel scan beams (default)\n";
  std::cout << "        equilinear    Equilinear divergent scan beams\n";
  std::cout << "        equiangular   Equiangular divergent scan beams\n";
  std::cout << "     --focal-length   Focal length ratio (ratio to radius of view area)\n";
  std::cout << "                      (default = 2)\n";
  std::cout << "     --center-detector-length  Distance from center of phantom to detector array\n";
  std::cout << "                      (ratio to radius of view area) (default = 2)\n";
  std::cout << "     --view-ratio     Length to view (view diameter to phantom diameter)\n";
  std::cout << "                      (default = 1)\n";
  std::cout << "     --scan-ratio     Length to scan (scan diameter to view diameter)\n";
  std::cout << "                      (default = 1)\n";
  std::cout << "     --offsetview     Initial gantry offset in 'views' (default = 0)\n";
  std::cout << "     --trace          Trace level to use\n";
  std::cout << "        none          No tracing (default)\n";
  std::cout << "        console       Trace text level\n";
  std::cout << "     --verbose        Verbose mode\n";
  std::cout << "     --debug          Debug mode\n";
  std::cout << "     --version        Print version\n";
  std::cout << "     --help           Print this help message\n";
}

#ifdef HAVE_MPI
void GatherProjectionsMPI (MPIWorld& mpiWorld, Projections& pjGlobal, Projections& pjLocal, const int opt_debug);
#endif

int
phm2pj_main (int argc, char* const argv[])
{
  Phantom phm;
  std::string optGeometryName = Scanner::convertGeometryIDToName(Scanner::GEOMETRY_PARALLEL);
  char *opt_outfile = NULL;
  std::string opt_desc;
  std::string optPhmName;
  std::string optPhmFileName;
  int opt_ndet;
  int opt_nview;
  int opt_offsetview = 0;
  int opt_nray = 1;
  double dOptFocalLength = 2.;
  double dOptCenterDetectorLength = 2.;
  double dOptViewRatio = 1.;
  double dOptScanRatio = 1.;
  int opt_trace = Trace::TRACE_NONE;
  int opt_verbose = 0;
  int opt_debug = 0;
  double opt_rotangle = -1;
  char* endptr = NULL;
  char* endstr;

#ifdef HAVE_MPI
  MPIWorld mpiWorld (argc, argv);
#endif

  Timer timerProgram;

#ifdef HAVE_MPI
  if (mpiWorld.getRank() == 0) {
#endif
    while (1) {
      int c = getopt_long(argc, argv, "", phm2pj_options, NULL);

      if (c == -1)
        break;

      switch (c) {
      case O_PHANTOM:
        optPhmName = optarg;
        break;
      case O_PHMFILE:
        optPhmFileName = optarg;
        break;
      case O_VERBOSE:
        opt_verbose = 1;
        break;
      case O_DEBUG:
        opt_debug = 1;
        break;
        break;
      case O_TRACE:
        if ((opt_trace = Trace::convertTraceNameToID(optarg)) == Trace::TRACE_INVALID) {
          phm2pj_usage(argv[0]);
          return (1);
        }
        break;
      case O_DESC:
        opt_desc = optarg;
        break;
      case O_ROTANGLE:
        opt_rotangle = strtod(optarg, &endptr);
        endstr = optarg + strlen(optarg);
        if (endptr != endstr) {
          std::cerr << "Error setting --rotangle to " << optarg << std::endl;
          phm2pj_usage(argv[0]);
          return (1);
        }
        break;
      case O_GEOMETRY:
        optGeometryName = optarg;
        break;
      case O_FOCAL_LENGTH:
        dOptFocalLength = strtod(optarg, &endptr);
        endstr = optarg + strlen(optarg);
        if (endptr != endstr) {
          std::cerr << "Error setting --focal-length to " << optarg << std::endl;
          phm2pj_usage(argv[0]);
          return (1);
        }
        break;
      case O_CENTER_DETECTOR_LENGTH:
        dOptCenterDetectorLength = strtod(optarg, &endptr);
        endstr = optarg + strlen(optarg);
        if (endptr != endstr) {
          std::cerr << "Error setting --center-detector-length to " << optarg << std::endl;
          phm2pj_usage(argv[0]);
          return (1);
        }
        break;
      case O_VIEW_RATIO:
        dOptViewRatio = strtod(optarg, &endptr);
        endstr = optarg + strlen(optarg);
        if (endptr != endstr) {
          std::cerr << "Error setting --view-ratio to " << optarg << std::endl;
          phm2pj_usage(argv[0]);
          return (1);
        }
        break;
      case O_SCAN_RATIO:
        dOptScanRatio = strtod(optarg, &endptr);
        endstr = optarg + strlen(optarg);
        if (endptr != endstr) {
          std::cerr << "Error setting --scan-ratio to " << optarg << std::endl;
          phm2pj_usage(argv[0]);
          return (1);
        }
        break;
      case O_NRAY:
        opt_nray = strtol(optarg, &endptr, 10);
        endstr = optarg + strlen(optarg);
        if (endptr != endstr) {
          std::cerr << "Error setting --nray to %s" << optarg << std::endl;
          phm2pj_usage(argv[0]);
          return (1);
        }
        break;
          case O_OFFSETVIEW:
                opt_offsetview = strtol(optarg, &endptr, 10);
                endstr = optarg + strlen(optarg);
                if (endptr != endstr) {
                  std::cerr << "Error setting --offsetview to %s" << optarg << std::endl;
                  phm2pj_usage(argv[0]);
                  return (1);
                }
                break;

      case O_VERSION:
#ifdef VERSION
        std::cout << "Version: " << VERSION << std::endl << g_szIdStr << std::endl;
#else
        std::cout << "Unknown version number\n";
#endif
        return (0);
      case O_HELP:
      case '?':
        phm2pj_usage(argv[0]);
        return (0);
      default:
        phm2pj_usage(argv[0]);
        return (1);
      }
    }

    if (optPhmName == "" && optPhmFileName == "") {
      std::cerr << "No phantom defined\n" << std::endl;
      phm2pj_usage(argv[0]);
      return (1);
    }
    if (optind + 3 != argc) {
      phm2pj_usage(argv[0]);
      return (1);
    }

    opt_outfile = argv[optind];
    opt_ndet = strtol(argv[optind+1], &endptr, 10);
    endstr = argv[optind+1] + strlen(argv[optind+1]);
    if (endptr != endstr) {
      std::cerr << "Error setting --ndet to " << argv[optind+1] << std::endl;
      phm2pj_usage(argv[0]);
      return (1);
    }
    opt_nview = strtol(argv[optind+2], &endptr, 10);
    endstr = argv[optind+2] + strlen(argv[optind+2]);
    if (endptr != endstr) {
      std::cerr << "Error setting --nview to " << argv[optind+2] << std::endl;
      phm2pj_usage(argv[0]);
      return (1);
    }

    if (opt_rotangle < 0) {
      if (optGeometryName.compare ("parallel") == 0)
        opt_rotangle = 0.5;
      else
        opt_rotangle = 1.0;
    }

    std::ostringstream desc;
    desc << "phm2pj: NDet=" << opt_ndet << ", Nview=" << opt_nview << ", NRay=" << opt_nray << ", RotAngle=" << opt_rotangle << "OffsetView =" << opt_offsetview << ", Geometry=" << optGeometryName << ", ";
    if (optPhmFileName.length()) {
      desc << "PhantomFile=" << optPhmFileName;
    } else if (optPhmName != "") {
      desc << "Phantom=" << optPhmName;
    }
    if (opt_desc.length()) {
      desc << ": " << opt_desc;
    }
    opt_desc = desc.str();

    if (optPhmName != "") {
      phm.createFromPhantom (optPhmName.c_str());
      if (phm.fail()) {
        std::cout << phm.failMessage() << std::endl << std::endl;
        phm2pj_usage(argv[0]);
        return (1);
      }
    }

    if (optPhmFileName != "") {
#ifdef HAVE_MPI
      std::cerr << "Can not read phantom from file in MPI mode\n";
      return (1);
#endif
      phm.createFromFile (optPhmFileName.c_str());
    }

#ifdef HAVE_MPI
  }
#endif

#ifdef HAVE_MPI
  TimerCollectiveMPI timerBcast(mpiWorld.getComm());
  mpiWorld.BcastString (optPhmName);
  mpiWorld.getComm().Bcast (&opt_rotangle, 1, MPI::DOUBLE, 0);
  mpiWorld.getComm().Bcast (&dOptFocalLength, 1, MPI::DOUBLE, 0);
  mpiWorld.getComm().Bcast (&dOptCenterDetectorLength, 1, MPI::DOUBLE, 0);
  mpiWorld.getComm().Bcast (&dOptViewRatio, 1, MPI::DOUBLE, 0);
  mpiWorld.getComm().Bcast (&dOptScanRatio, 1, MPI::DOUBLE, 0);
  mpiWorld.getComm().Bcast (&opt_nview, 1, MPI::INT, 0);
  mpiWorld.getComm().Bcast (&opt_ndet, 1, MPI::INT, 0);
  mpiWorld.getComm().Bcast (&opt_nray, 1, MPI::INT, 0);
  mpiWorld.getComm().Bcast (&opt_verbose, 1, MPI::INT, 0);
  mpiWorld.getComm().Bcast (&opt_debug, 1, MPI::INT, 0);
  mpiWorld.getComm().Bcast (&opt_trace, 1, MPI::INT, 0);
  if (opt_verbose)
    timerBcast.timerEndAndReport ("Time to broadcast variables");

  if (mpiWorld.getRank() > 0 && optPhmName != "")
    phm.createFromPhantom (optPhmName.c_str());
#endif

  opt_rotangle *= TWOPI;
  Scanner scanner (phm, optGeometryName.c_str(), opt_ndet, opt_nview, opt_offsetview, opt_nray,
                opt_rotangle, dOptFocalLength, dOptCenterDetectorLength, dOptViewRatio, dOptScanRatio);
  if (scanner.fail()) {
    std::cout << "Scanner Creation Error: " << scanner.failMessage() << std::endl;
    return (1);
  }
#ifdef HAVE_MPI
  mpiWorld.setTotalWorkUnits (opt_nview);

  Projections pjGlobal;
  if (mpiWorld.getRank() == 0)
    pjGlobal.initFromScanner (scanner);

  if (opt_verbose) {
    std::ostringstream os;
    pjGlobal.printScanInfo(os);
    std::cout << os.str();
  }

  Projections pjLocal (scanner);
  pjLocal.setNView (mpiWorld.getMyLocalWorkUnits());

  if (opt_debug)
    std::cout << "pjLocal->nview = " << pjLocal.nView() << " (process " << mpiWorld.getRank() << ")\n";;

  TimerCollectiveMPI timerProject (mpiWorld.getComm());
  scanner.collectProjections (pjLocal, phm, mpiWorld.getMyStartWorkUnit(), mpiWorld.getMyLocalWorkUnits(), false, opt_trace);
  if (opt_verbose)
    timerProject.timerEndAndReport ("Time to collect projections");

  TimerCollectiveMPI timerGather (mpiWorld.getComm());
  GatherProjectionsMPI (mpiWorld, pjGlobal, pjLocal, opt_debug);
  if (opt_verbose)
    timerGather.timerEndAndReport ("Time to gather projections");

#else
  Projections pjGlobal (scanner);
  scanner.collectProjections (pjGlobal, phm, 0, opt_nview, opt_offsetview, true, opt_trace);
#endif

#ifdef HAVE_MPI
  if (mpiWorld.getRank() == 0)
#endif
  {
    pjGlobal.setCalcTime (timerProgram.timerEnd());
    pjGlobal.setRemark (opt_desc);
    pjGlobal.write (opt_outfile);
    if (opt_verbose) {
      phm.print (std::cout);
      std::cout << std::endl;
      std::ostringstream os;
      pjGlobal.printScanInfo (os);
      std::cout << os.str() << std::endl;
      std::cout << "  Remark: " << pjGlobal.remark() << std::endl;
      std::cout << "Run time: " << pjGlobal.calcTime() << " seconds\n";
    }
  }

  return (0);
}


/* FUNCTION
*    GatherProjectionsMPI
*
* SYNOPSIS
*    Gather's raysums from all processes in pjLocal in pjGlobal in process 0
*/

#ifdef HAVE_MPI
void GatherProjectionsMPI (MPIWorld& mpiWorld, Projections& pjGlobal, Projections& pjLocal, const int opt_debug)
{
  for (int iw = 0; iw < mpiWorld.getMyLocalWorkUnits(); iw++) {
    DetectorArray& detArray = pjLocal.getDetectorArray(iw);
    double viewAngle = detArray.viewAngle();
    int nDet = detArray.nDet();
    DetectorValue* detval = detArray.detValues();

    mpiWorld.getComm().Send(&viewAngle, 1, MPI::DOUBLE, 0, 0);
    mpiWorld.getComm().Send(&nDet, 1, MPI::INT, 0, 0);
    mpiWorld.getComm().Send(detval, nDet, MPI::FLOAT, 0, 0);
  }

  if (mpiWorld.getRank() == 0) {
    for (int iProc = 0; iProc < mpiWorld.getNumProcessors(); iProc++) {
      for (int iw = mpiWorld.getStartWorkUnit(iProc); iw <= mpiWorld.getEndWorkUnit(iProc); iw++) {
        MPI::Status status;
        double viewAngle;
        int nDet;
        DetectorArray& detArray = pjGlobal.getDetectorArray(iw);
        DetectorValue* detval = detArray.detValues();

        mpiWorld.getComm().Recv(&viewAngle, 1, MPI::DOUBLE, iProc, 0, status);
        mpiWorld.getComm().Recv(&nDet, 1, MPI::INT, iProc, 0, status);
        mpiWorld.getComm().Recv(detval, nDet, MPI::FLOAT, iProc, 0, status);
        detArray.setViewAngle (viewAngle);
      }
    }
  }
}
#endif


#ifndef NO_MAIN
int
main (int argc, char* argv[])
{
  int retval = 1;

  try {
    retval = phm2pj_main(argc, argv);
#if HAVE_DMALLOC
    //    dmalloc_shutdown();
#endif
  } catch (exception e) {
    std::cerr << "Exception: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << "Unknown exception\n";
  }

  return (retval);
}
#endif

