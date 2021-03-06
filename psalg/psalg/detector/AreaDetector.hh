#ifndef PSALG_AREADETECTOR_H
#define PSALG_AREADETECTOR_H
//-----------------------------

#include <string>
#include "psalg/calib/CalibParsStore.hh" // CalibPars, getCalibPars
#include "psalg/calib/NDArray.hh" // NDArray
#include "psalg/calib/AreaDetectorTypes.hh"
#include "psalg/detector/Detector.hh"

using namespace std;
using namespace psalg;

namespace detector {

//-----------------------------

class AreaDetector : public Detector {
public:

  AreaDetector(const std::string& detname);
  virtual ~AreaDetector();

  void _default_msg(const std::string& msg=std::string()) const;

  /// shape, size, ndim of data from configuration object
  virtual const shape_t* shape(const event_t&);
  virtual const size_t   ndim (const event_t&);
  virtual const size_t   size (const event_t&);

  /// access to calibration constants
  virtual const NDArray<common_mode_t>&   common_mode      (const event_t&);
  virtual const NDArray<pedestals_t>&     pedestals        (const event_t&);
  virtual const NDArray<pixel_rms_t>&     rms              (const event_t&);
  virtual const NDArray<pixel_status_t>&  status           (const event_t&);
  virtual const NDArray<pixel_gain_t>&    gain             (const event_t&);
  virtual const NDArray<pixel_offset_t>&  offset           (const event_t&);
  virtual const NDArray<pixel_bkgd_t>&    background       (const event_t&);
  virtual const NDArray<pixel_mask_t>&    mask_calib       (const event_t&);
  virtual const NDArray<pixel_mask_t>&    mask_from_status (const event_t&);
  virtual const NDArray<pixel_mask_t>&    mask_edges       (const event_t&, const size_t& nnbrs=8);
  virtual const NDArray<pixel_mask_t>&    mask_neighbors   (const event_t&, const size_t& nrows=1, const size_t& ncols=1);
  virtual const NDArray<pixel_mask_t>&    mask_bits        (const event_t&, const size_t& mbits=0177777);
  virtual const NDArray<pixel_mask_t>&    mask             (const event_t&, const bool& calib=true,
							                    const bool& sataus=true,
                                                                            const bool& edges=true,
							                    const bool& neighbors=true);

  /// access to raw, calibrated data, and image
  virtual const NDArray<raw_t>& raw(const event_t&);
  virtual const NDArray<calib_t>& calib(const event_t&);
  virtual const NDArray<image_t>& image(const event_t&);
  virtual const NDArray<image_t>& image(const event_t&, const NDArray<image_t>& nda);
  virtual const NDArray<image_t>& array_from_image(const event_t&, const NDArray<image_t>&);
  virtual void move_geo(const event_t&, const pixel_size_t& dx,  const pixel_size_t& dy,  const pixel_size_t& dz);
  virtual void tilt_geo(const event_t&, const tilt_angle_t& dtx, const tilt_angle_t& dty, const tilt_angle_t& dtz);

  /// access to geometry
  virtual const geometry_t& geometry(const event_t&);
  virtual const NDArray<pixel_idx_t>&   indexes    (const event_t&, const size_t& axis=0);
  virtual const NDArray<pixel_coord_t>& coords     (const event_t&, const size_t& axis=0);
  virtual const NDArray<pixel_size_t>&  pixel_size (const event_t&, const size_t& axis=0);
  virtual const NDArray<pixel_size_t>&  image_xaxis(const event_t&);
  virtual const NDArray<pixel_size_t>&  image_yaxis(const event_t&);

  calib::CalibPars* calib_pars();
  calib::CalibPars* calib_pars_updated();

  AreaDetector(const AreaDetector&) = delete;
  AreaDetector& operator = (const AreaDetector&) = delete;
  AreaDetector(){}

protected:
  shape_t* _shape;

private:

  calib::CalibPars*       _calib_pars;

  //std::string _detname;
  NDArray<raw_t>          _raw;
  NDArray<calib_t>        _calib;
  NDArray<image_t>        _image;

  /*
  NDArray<common_mode_t>  _common_mode;
  NDArray<pedestals_t>    _pedestals;

  NDArray<pixel_rms_t>    _pixel_rms;
  NDArray<pixel_status_t> _pixel_status;
  NDArray<pixel_gain_t>   _pixel_gain;
  NDArray<pixel_offset_t> _pixel_offset;
  NDArray<pixel_bkgd_t>   _pixel_bkgd;
  NDArray<pixel_mask_t>   _pixel_mask;

  geometry_t              _geometry;
  NDArray<pixel_idx_t>    _pixel_idx;
  NDArray<pixel_coord_t>  _pixel_coord;
  NDArray<pixel_size_t>   _pixel_size;
  */
}; // class

} // namespace detector

#endif // PSALG_AREADETECTOR_H
//-----------------------------
