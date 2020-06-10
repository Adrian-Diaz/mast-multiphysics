

#ifndef _mast_quadrature_h_
#define _mast_quadrature_h_

// MAST includes
#include "base/compute_kernel.h"


namespace MAST {

template <typename ScalarType, typename ContextType>
class Quadrature: public MAST::ComputeKernelBase<ContextType> {
    
public:

    using scalar_type = ScalarType;
    
    Quadrature(const std::string& nm, bool executable):
    MAST::ComputeKernelBase<ContextType>(nm, executable) {}
    virtual ~Quadrature() {}
    virtual inline uint_type dim() const = 0;
    virtual inline uint_type order() const = 0;
    virtual inline uint_type n_points() const = 0;
    virtual inline ScalarType qp_coord(uint_type qp, uint_type xi_i) const = 0;
    virtual inline ScalarType weight(uint_type qp) const = 0;
    
};



/*! This provides a  */
template <typename ScalarType, typename Traits, typename ContextType>
class MappedQuadrature: public MAST::Quadrature<ScalarType, ContextType> {
    
public:

    using scalar_type = ScalarType;
    
    MappedQuadrature(const std::string& nm):
    MAST::Quadrature<ScalarType, ContextType>(nm), _dim(0) {}
    virtual ~MappedQuadrature() {}
    virtual void set_data(const uint_type dim,
                          const typename Eigen::Matrix<ScalarType, Dynamic, Dynamic>& points,
                          const typename Eigen::Matrix<ScalarType, Dynamic, 1>& weights) {
        _points  = points;
        _weights = weights;
    }
    virtual inline uint_type dim() const override { return _dim;}
    virtual inline uint_type order() const override { /* should not be called*/ libmesh_assert(false);}
    virtual inline uint_type n_points() const override { return _weights.size();}
    virtual inline ScalarType qp_coord(uint_type qp, uint_type xi_i) const override { return _points(qp, xi_i);}
    virtual inline ScalarType weight(uint_type qp) const { return _weights(qp);}
    
protected:

    uint_type _dim;
    Eigen::Matrix<ScalarType, Dynamic, Dynamic> _points;
    Eigen::Matrix<ScalarType, Dynamic, 1> _weights;
};



/*! serves as a wrapper around libMesh */
template <typename ContextType>
class libMeshQuadrature: public MAST::Quadrature<Real, ContextType> {
    
public:
    
    using scalar_type = typename MAST::Quadrature<Real, ContextType>::scalar_type;
    
    /*! the quadrature object \p q  is expected to be initialized outside of this class. */
    libMeshQuadrature(const std::string& nm):
    MAST::Quadrature<Real, ContextType>(nm, false),
    _q  (nullptr)
    {}
    
    virtual ~libMeshQuadrature() {}
    
    virtual inline void set_quadrature(const libMesh::QBase& q) { _q = &q; }
    
    const libMesh::QBase& get_libmesh_object() const {
        
        libmesh_assert_msg(_q, "Quadrature object not initialized");
        return *_q;
    }

    virtual inline uint_type dim() const override {
        
        libmesh_assert_msg(_q, "Quadrature object not initialized");
        return _q->get_dim();
    }
    
    virtual inline uint_type order() const override {
        
        libmesh_assert_msg(_q, "Quadrature object not initialized");
        return _q->get_order();
    }
    
    virtual inline uint_type n_points() const override {
        
        libmesh_assert_msg(_q, "Quadrature object not initialized");
        return _q->n_points();
    }
    
    virtual inline scalar_type qp_coord(uint_type qp, uint_type xi_i) const override {
        
        libmesh_assert_msg(_q, "Quadrature object not initialized");
        return _q->get_points()[qp](xi_i);
    }
    
    virtual inline scalar_type weight(uint_type qp) const override {
        
        libmesh_assert_msg(_q, "Quadrature object not initialized");
        return _q->w(qp);
    }
    
protected:
    
    const libMesh::QBase* _q;
};
}

#endif  //_mast_quadrature_h_