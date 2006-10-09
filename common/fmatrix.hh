// -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi: set et ts=4 sw=2 sts=2:
// $Id$
#ifndef DUNE_FMATRIX_HH
#define DUNE_FMATRIX_HH

#include <cmath>
#include <cstddef>
#include <complex>
#include <iostream>
#include "exceptions.hh"
#include "fvector.hh"
#include "precision.hh"

namespace Dune {

  /**
              @addtogroup DenseMatVec
              @{
   */

  /*! \file

     \brief  This file implements a matrix constructed from a given type
     representing a field and compile-time given number of rows and columns.
   */

  template<class K, int n, int m> class FieldMatrix;

  /** @brief Error thrown if operations of a FieldMatrix fail. */
  class FMatrixError : public Exception {};

  // conjugate komplex does nothing for non-complex types
  template<class K>
  inline K fm_ck (const K& k)
  {
    return k;
  }

  // conjugate komplex
  template<class K>
  inline std::complex<K> fm_ck (const std::complex<K>& c)
  {
    return std::complex<K>(c.real(),-c.imag());
  }


  //! solve small system
  template<class K, int n, class V>
  void fm_solve (const FieldMatrix<K,n,n>& Ain,  V& x, const V& b)
  {
    // make a copy of a to store factorization
    FieldMatrix<K,n,n> A(Ain);

    // Gaussian elimination with maximum column pivot
    double norm=A.infinity_norm_real();     // for relative thresholds
    double pivthres = std::max(FMatrixPrecision<>::absolute_limit(),norm*FMatrixPrecision<>::pivoting_limit());
    double singthres = std::max(FMatrixPrecision<>::absolute_limit(),norm*FMatrixPrecision<>::singular_limit());
    V& rhs = x;          // use x to store rhs
    rhs = b;             // copy data

    // elimination phase
    for (int i=0; i<n; i++)      // loop over all rows
    {
      double pivmax=fvmeta_absreal(A[i][i]);

      // pivoting ?
      if (pivmax<pivthres)
      {
        // compute maximum of row
        int imax=i; double abs;
        for (int k=i+1; k<n; k++)
          if ((abs=fvmeta_absreal(A[k][i]))>pivmax)
          {
            pivmax = abs; imax = k;
          }
        // swap rows
        if (imax!=i)
          for (int j=i; j<n; j++)
            std::swap(A[i][j],A[imax][j]);
      }

      // singular ?
      if (pivmax<singthres)
        DUNE_THROW(FMatrixError,"matrix is singular");

      // eliminate
      for (int k=i+1; k<n; k++)
      {
        K factor = -A[k][i]/A[i][i];
        for (int j=i+1; j<n; j++)
          A[k][j] += factor*A[i][j];
        rhs[k] += factor*rhs[i];
      }
    }

    // backsolve
    for (int i=n-1; i>=0; i--)
    {
      for (int j=i+1; j<n; j++)
        rhs[i] -= A[i][j]*x[j];
      x[i] = rhs[i]/A[i][i];
    }
  }

  //! special case for 1x1 matrix, x and b may be identical
  template<class K, class V>
  inline void fm_solve (const FieldMatrix<K,1,1>& A,  V& x, const V& b)
  {
#ifdef DUNE_FMatrix_WITH_CHECKING
    if (fvmeta_absreal(A[0][0])<FMatrixPrecision<>::absolute_limit())
      DUNE_THROW(FMatrixError,"matrix is singular");
#endif
    x[0] = b[0]/A[0][0];
  }

  //! special case for 2x2 matrix, x and b may be identical
  template<class K, class V>
  inline void fm_solve (const FieldMatrix<K,2,2>& A,  V& x, const V& b)
  {
#ifdef DUNE_FMatrix_WITH_CHECKING
    K detinv = A[0][0]*A[1][1]-A[0][1]*A[1][0];
    if (fvmeta_absreal(detinv)<FMatrixPrecision<>::absolute_limit())
      DUNE_THROW(FMatrixError,"matrix is singular");
    detinv = 1/detinv;
#else
    K detinv = 1.0/(A[0][0]*A[1][1]-A[0][1]*A[1][0]);
#endif

    K temp = b[0];
    x[0] = detinv*(A[1][1]*b[0]-A[0][1]*b[1]);
    x[1] = detinv*(A[0][0]*b[1]-A[1][0]*temp);
  }



  //! compute inverse
  template<class K, int n>
  void fm_invert (FieldMatrix<K,n,n>& B)
  {
    FieldMatrix<K,n,n> A(B);
    FieldMatrix<K,n,n>& L=A;
    FieldMatrix<K,n,n>& U=A;

    double norm=A.infinity_norm_real();     // for relative thresholds
    double pivthres = std::max(FMatrixPrecision<>::absolute_limit(),norm*FMatrixPrecision<>::pivoting_limit());
    double singthres = std::max(FMatrixPrecision<>::absolute_limit(),norm*FMatrixPrecision<>::singular_limit());

    // LU decomposition of A in A
    for (int i=0; i<n; i++)      // loop over all rows
    {
      double pivmax=fvmeta_absreal(A[i][i]);

      // pivoting ?
      if (pivmax<pivthres)
      {
        // compute maximum of column
        int imax=i; double abs;
        for (int k=i+1; k<n; k++)
          if ((abs=fvmeta_absreal(A[k][i]))>pivmax)
          {
            pivmax = abs; imax = k;
          }
        // swap rows
        if (imax!=i)
          for (int j=i; j<n; j++)
            std::swap(A[i][j],A[imax][j]);
      }

      // singular ?
      if (pivmax<singthres)
        DUNE_THROW(FMatrixError,"matrix is singular");

      // eliminate
      for (int k=i+1; k<n; k++)
      {
        K factor = A[k][i]/A[i][i];
        L[k][i] = factor;
        for (int j=i+1; j<n; j++)
          A[k][j] -= factor*A[i][j];
      }
    }

    // initialize inverse
    B = 0;
    for (int i=0; i<n; i++) B[i][i] = 1;

    // L Y = I; multiple right hand sides
    for (int i=0; i<n; i++)
      for (int j=0; j<i; j++)
        for (int k=0; k<n; k++)
          B[i][k] -= L[i][j]*B[j][k];

    // U A^{-1} = Y
    for (int i=n-1; i>=0; i--)
      for (int k=0; k<n; k++)
      {
        for (int j=i+1; j<n; j++)
          B[i][k] -= U[i][j]*B[j][k];
        B[i][k] /= U[i][i];
      }
  }

  //! compute inverse n=1
  template<class K>
  void fm_invert (FieldMatrix<K,1,1>& A)
  {
#ifdef DUNE_FMatrix_WITH_CHECKING
    if (fvmeta_absreal(A[0][0])<FMatrixPrecision<>::absolute_limit())
      DUNE_THROW(FMatrixError,"matrix is singular");
#endif
    A[0][0] = 1/A[0][0];
  }

  //! compute inverse n=2
  template<class K>
  void fm_invert (FieldMatrix<K,2,2>& A)
  {
    K detinv = A[0][0]*A[1][1]-A[0][1]*A[1][0];
#ifdef DUNE_FMatrix_WITH_CHECKING
    if (fvmeta_absreal(detinv)<FMatrixPrecision<>::absolute_limit())
      DUNE_THROW(FMatrixError,"matrix is singular");
#endif
    detinv = 1/detinv;

    K temp=A[0][0];
    A[0][0] =  A[1][1]*detinv;
    A[0][1] = -A[0][1]*detinv;
    A[1][0] = -A[1][0]*detinv;
    A[1][1] =  temp*detinv;
  }


  /**
      @brief A dense n x m matrix.

     Matrices represent linear maps from a vector space V to a vector space W.
       This class represents such a linear map by storing a two-dimensional
       array of numbers of a given field type K. The number of rows and
       columns is given at compile time.

           Implementation of all members uses template meta programs where appropriate
   */
#ifdef DUNE_EXPRESSIONTEMPLATES
  template<class K, int n, int m>
  class FieldMatrix : ExprTmpl::Matrix< FieldMatrix<K,n,m> >
#else
  template<class K, int n, int m>
  class FieldMatrix
#endif
  {
  public:
    // standard constructor and everything is sufficient ...

    //===== type definitions and constants

    //! export the type representing the field
    typedef K field_type;

    //! export the type representing the components
    typedef K block_type;

    //! The type used for the index access and size operations.
    typedef std::size_t size_type;

    //! We are at the leaf of the block recursion
    enum {
      //! The number of block levels we contain. This is 1.
      blocklevel = 1
    };

    //! Each row is implemented by a field vector
    typedef FieldVector<K,m> row_type;

    //! export size
    enum {
      //! The number of rows.
      rows = n,
      //! The number of columns.
      cols = m
    };

    //===== constructors
    /** \brief Default constructor
     */
    FieldMatrix () {}

    /** \brief Constructor initializing the whole matrix with a scalar
     */
    FieldMatrix (const K& k)
    {
      for (size_type i=0; i<n; i++) p[i] = k;
    }

    //===== random access interface to rows of the matrix

    //! random access to the rows
    row_type& operator[] (size_type i)
    {
#ifdef DUNE_FMatrix_WITH_CHECKING
      if (i<0 || i>=n) DUNE_THROW(FMatrixError,"index out of range");
#endif
      return p[i];
    }

    //! same for read only access
    const row_type& operator[] (size_type i) const
    {
#ifdef DUNE_FMatrix_WITH_CHECKING
      if (i<0 || i>=n) DUNE_THROW(FMatrixError,"index out of range");
#endif
      return p[i];
    }


    //===== iterator interface to rows of the matrix
    //! Iterator class for sequential access
    typedef FieldIterator<FieldMatrix<K,n,m>,row_type> Iterator;
    //! typedef for stl compliant access
    typedef Iterator iterator;
    //! rename the iterators for easier access
    typedef Iterator RowIterator;
    //! rename the iterators for easier access
    typedef typename row_type::Iterator ColIterator;

    //! begin iterator
    Iterator begin ()
    {
      return Iterator(*this,0);
    }

    //! end iterator
    Iterator end ()
    {
      return Iterator(*this,n);
    }

    //! begin iterator
    Iterator rbegin ()
    {
      return Iterator(*this,n-1);
    }

    //! end iterator
    Iterator rend ()
    {
      return Iterator(*this,-1);
    }

    //! Iterator class for sequential access
    typedef FieldIterator<const FieldMatrix<K,n,m>,const row_type> ConstIterator;
    //! typedef for stl compliant access
    typedef ConstIterator const_iterator;
    //! rename the iterators for easier access
    typedef ConstIterator ConstRowIterator;
    //! rename the iterators for easier access
    typedef typename row_type::ConstIterator ConstColIterator;

    //! begin iterator
    ConstIterator begin () const
    {
      return ConstIterator(*this,0);
    }

    //! end iterator
    ConstIterator end () const
    {
      return ConstIterator(*this,n);
    }

    //! begin iterator
    ConstIterator rbegin () const
    {
      return ConstIterator(*this,n-1);
    }

    //! end iterator
    ConstIterator rend () const
    {
      return ConstIterator(*this,-1);
    }

    //===== assignment from scalar
    FieldMatrix& operator= (const K& k)
    {
      for (int i=0; i<n; i++)
        p[i] = k;
      return *this;
    }

    //===== vector space arithmetic

    //! vector space addition
    FieldMatrix& operator+= (const FieldMatrix& y)
    {
      for (int i=0; i<n; i++)
        p[i] += y.p[i];
      return *this;
    }

    //! vector space subtraction
    FieldMatrix& operator-= (const FieldMatrix& y)
    {
      for (int i=0; i<n; i++)
        p[i] -= y.p[i];
      return *this;
    }

    //! vector space multiplication with scalar
    FieldMatrix& operator*= (const K& k)
    {
      for (int i=0; i<n; i++)
        p[i] *= k;
      return *this;
    }

    //! vector space division by scalar
    FieldMatrix& operator/= (const K& k)
    {
      for (int i=0; i<n; i++)
        p[i] /= k;
      return *this;
    }

    //===== linear maps

    //! y += A x
    template<class X, class Y>
    void umv (const X& x, Y& y) const
    {
#ifdef DUNE_FMatrix_WITH_CHECKING
      if (x.N()!=M()) DUNE_THROW(FMatrixError,"index out of range");
      if (y.N()!=N()) DUNE_THROW(FMatrixError,"index out of range");
#endif
      for (size_type i=0; i<n; i++)
        for (size_type j=0; j<m; j++)
          y[i] += (*this)[i][j] * x[j];
    }

    //! y += A^T x
    template<class X, class Y>
    void umtv (const X& x, Y& y) const
    {
#ifdef DUNE_FMatrix_WITH_CHECKING
      if (x.N()!=N()) DUNE_THROW(FMatrixError,"index out of range");
      if (y.N()!=M()) DUNE_THROW(FMatrixError,"index out of range");
#endif

      for (size_type i=0; i<n; i++)
        for (size_type j=0; j<m; j++)
          y[j] += p[i][j]*x[i];
    }

    //! y += A^H x
    template<class X, class Y>
    void umhv (const X& x, Y& y) const
    {
#ifdef DUNE_FMatrix_WITH_CHECKING
      if (x.N()!=N()) DUNE_THROW(FMatrixError,"index out of range");
      if (y.N()!=M()) DUNE_THROW(FMatrixError,"index out of range");
#endif

      for (size_type i=0; i<n; i++)
        for (size_type j=0; j<m; j++)
          y[j] += fm_ck(p[i][j])*x[i];
    }

    //! y -= A x
    template<class X, class Y>
    void mmv (const X& x, Y& y) const
    {
#ifdef DUNE_FMatrix_WITH_CHECKING
      if (x.N()!=M()) DUNE_THROW(FMatrixError,"index out of range");
      if (y.N()!=N()) DUNE_THROW(FMatrixError,"index out of range");
#endif
      for (size_type i=0; i<n; i++)
        for (size_type j=0; j<m; j++)
          y[i] -= (*this)[i][j] * x[j];
    }

    //! y -= A^T x
    template<class X, class Y>
    void mmtv (const X& x, Y& y) const
    {
#ifdef DUNE_FMatrix_WITH_CHECKING
      if (x.N()!=N()) DUNE_THROW(FMatrixError,"index out of range");
      if (y.N()!=M()) DUNE_THROW(FMatrixError,"index out of range");
#endif

      for (size_type i=0; i<n; i++)
        for (size_type j=0; j<m; j++)
          y[j] -= p[i][j]*x[i];
    }

    //! y -= A^H x
    template<class X, class Y>
    void mmhv (const X& x, Y& y) const
    {
#ifdef DUNE_FMatrix_WITH_CHECKING
      if (x.N()!=N()) DUNE_THROW(FMatrixError,"index out of range");
      if (y.N()!=M()) DUNE_THROW(FMatrixError,"index out of range");
#endif

      for (size_type i=0; i<n; i++)
        for (size_type j=0; j<m; j++)
          y[j] -= fm_ck(p[i][j])*x[i];
    }

    //! y += alpha A x
    template<class X, class Y>
    void usmv (const K& alpha, const X& x, Y& y) const
    {
#ifdef DUNE_FMatrix_WITH_CHECKING
      if (x.N()!=M()) DUNE_THROW(FMatrixError,"index out of range");
      if (y.N()!=N()) DUNE_THROW(FMatrixError,"index out of range");
#endif
      for (size_type i=0; i<n; i++)
        for (size_type j=0; j<m; j++)
          y[i] += alpha * (*this)[i][j] * x[j];
    }

    //! y += alpha A^T x
    template<class X, class Y>
    void usmtv (const K& alpha, const X& x, Y& y) const
    {
#ifdef DUNE_FMatrix_WITH_CHECKING
      if (x.N()!=N()) DUNE_THROW(FMatrixError,"index out of range");
      if (y.N()!=M()) DUNE_THROW(FMatrixError,"index out of range");
#endif

      for (size_type i=0; i<n; i++)
        for (size_type j=0; j<m; j++)
          y[j] += alpha*p[i][j]*x[i];
    }

    //! y += alpha A^H x
    template<class X, class Y>
    void usmhv (const K& alpha, const X& x, Y& y) const
    {
#ifdef DUNE_FMatrix_WITH_CHECKING
      if (x.N()!=N()) DUNE_THROW(FMatrixError,"index out of range");
      if (y.N()!=M()) DUNE_THROW(FMatrixError,"index out of range");
#endif

      for (size_type i=0; i<n; i++)
        for (size_type j=0; j<m; j++)
          y[j] += alpha*fm_ck(p[i][j])*x[i];
    }

    //===== norms

    //! frobenius norm: sqrt(sum over squared values of entries)
    double frobenius_norm () const
    {
      double sum=0;
      for (size_type i=0; i<n; ++i) sum += p[i].two_norm2();
      return sqrt(sum);
    }

    //! square of frobenius norm, need for block recursion
    double frobenius_norm2 () const
    {
      double sum=0;
      for (size_type i=0; i<n; ++i) sum += p[i].two_norm2();
      return sum;
    }

    //! infinity norm (row sum norm, how to generalize for blocks?)
    double infinity_norm () const
    {
      double max=0;
      for (size_type i=0; i<n; ++i) max = std::max(max,p[i].one_norm());
      return max;
    }

    //! simplified infinity norm (uses Manhattan norm for complex values)
    double infinity_norm_real () const
    {
      double max=0;
      for (size_type i=0; i<n; ++i) max = std::max(max,p[i].one_norm_real());
      return max;
    }

    //===== solve

    /** \brief Solve system A x = b
     *
     * \exception FMatrixError if the matrix is singular
     */
    template<class V>
    void solve (V& x, const V& b) const
    {
      fm_solve(*this,x,b);
    }

    /** \brief Compute inverse
     *
     * \exception FMatrixError if the matrix is singular
     */
    void invert ()
    {
      fm_invert(*this);
    }

    //! calculates the determinant of this matrix
    K determinant () const;

    //! Multiplies M from the left to this matrix
    FieldMatrix& leftmultiply (const FieldMatrix<K,n,n>& M)
    {
      FieldMatrix<K,n,m> C(*this);

      for (int i=0; i<n; i++)
        for (int j=0; j<m; j++) {
          (*this)[i][j] = 0;
          for (int k=0; k<n; k++)
            (*this)[i][j] += M[i][k]*C[k][j];
        }

      return *this;
    }

    //! Multiplies M from the right to this matrix
    FieldMatrix& rightmultiply (const FieldMatrix<K,n,n>& M)
    {
      FieldMatrix<K,n,m> C(*this);

      for (int i=0; i<n; i++)
        for (int j=0; j<m; j++) {
          (*this)[i][j] = 0;
          for (int k=0; k<m; k++)
            (*this)[i][j] += C[i][k]*M[k][j];
        }
      return *this;
    }


    //===== sizes

    //! number of blocks in row direction
    size_type N () const
    {
      return n;
    }

    //! number of blocks in column direction
    size_type M () const
    {
      return m;
    }

    //! row dimension of block r
    size_type rowdim (size_type r) const
    {
      return 1;
    }

    //! col dimension of block c
    size_type coldim (size_type c) const
    {
      return 1;
    }

    //! dimension of the destination vector space
    size_type rowdim () const
    {
      return n;
    }

    //! dimension of the source vector space
    size_type coldim () const
    {
      return m;
    }

    //===== query

    //! return true when (i,j) is in pattern
    bool exists (size_type i, size_type j) const
    {
#ifdef DUNE_FMatrix_WITH_CHECKING
      if (i<0 || i>=n) DUNE_THROW(FMatrixError,"index out of range");
      if (j<0 || i>=m) DUNE_THROW(FMatrixError,"index out of range");
#endif
      return true;
    }

    //===== conversion operator

    /** \brief Sends the matrix to an output stream */
    friend std::ostream& operator<< (std::ostream& s, const FieldMatrix<K,n,m>& a)
    {
      for (size_type i=0; i<n; i++)
        s << a.p[i] << std::endl;
      return s;
    }

  private:
    // the data, very simply a built in array with row-wise ordering
    row_type p[(n > 0) ? n : 1];
  };


  namespace HelpMat {

    // calculation of determinat of matrix
    template <class K, int row,int col>
    static inline K determinantMatrix (const FieldMatrix<K,row,col> &matrix)
    {
      if (row!=col)
        DUNE_THROW(FMatrixError, "There is no determinant for a " << row << "x" << col << " matrix!");

      DUNE_THROW(FMatrixError, "No implementation of determinantMatrix "
                 << "for FieldMatrix<" << row << "," << col << "> !");

      return 0.0;
    }

    template <typename K>
    static inline K determinantMatrix (const FieldMatrix<K,1,1> &matrix)
    {
      return matrix[0][0];
    }

    template <typename K>
    static inline K determinantMatrix (const FieldMatrix<K,2,2> &matrix)
    {
      return matrix[0][0]*matrix[1][1] - matrix[0][1]*matrix[1][0];
    }

    template <typename K>
    static inline K determinantMatrix (const FieldMatrix<K,3,3> &matrix)
    {
      // code generated by maple
      K t4  = matrix[0][0] * matrix[1][1];
      K t6  = matrix[0][0] * matrix[1][2];
      K t8  = matrix[0][1] * matrix[1][0];
      K t10 = matrix[0][2] * matrix[1][0];
      K t12 = matrix[0][1] * matrix[2][0];
      K t14 = matrix[0][2] * matrix[2][0];

      K det = (t4*matrix[2][2]-t6*matrix[2][1]-t8*matrix[2][2]+
               t10*matrix[2][1]+t12*matrix[1][2]-t14*matrix[1][1]);
      return det;
    }

  } // end namespace HelpMat

  // implementation of the determinant
  template <class K, int n, int m>
  inline K FieldMatrix<K,n,m>::determinant () const
  {
    return HelpMat::determinantMatrix(*this);
  }


  /** \brief Special type for 1x1 matrices
   */
  template<class K>
  class FieldMatrix<K,1,1>
  {
  public:
    // standard constructor and everything is sufficient ...

    //===== type definitions and constants

    //! export the type representing the field
    typedef K field_type;

    //! export the type representing the components
    typedef K block_type;

    //! The type used for index access and size operations
    typedef std::size_t size_type;

    //! We are at the leaf of the block recursion
    enum {
      //! The number of block levels we contain.
      //! This is always one for this type.
      blocklevel = 1
    };

    //! Each row is implemented by a field vector
    typedef FieldVector<K,1> row_type;

    //! export size
    enum {
      //! \brief The number of rows.
      //! This is always one for this type.
      rows = 1,
      n = 1,
      //! \brief The number of columns.
      //! This is always one for this type.
      cols = 1,
      m = 1
    };

    //===== constructors
    /** \brief Default constructor
     */
    FieldMatrix () {}

    /** \brief Constructor initializing the whole matrix with a scalar
     */
    FieldMatrix (const K& k)
    {
      a = k;
    }

    //===== random access interface to rows of the matrix

    //! random access to the rows
    row_type& operator[] (size_type i)
    {
#ifdef DUNE_FMatrix_WITH_CHECKING
      if (i<0 || i>=n) DUNE_THROW(FMatrixError,"index out of range");
#endif
      return a;
    }

    //! same for read only access
    const row_type& operator[] (size_type i) const
    {
#ifdef DUNE_FMatrix_WITH_CHECKING
      if (i<0 || i>=n) DUNE_THROW(FMatrixError,"index out of range");
#endif
      return a;
    }

    //===== iterator interface to rows of the matrix
    //! Iterator class for sequential access
    typedef FieldIterator<FieldMatrix<K,n,m>,row_type> Iterator;
    //! typedef for stl compliant access
    typedef Iterator iterator;
    //! rename the iterators for easier access
    typedef Iterator RowIterator;
    //! rename the iterators for easier access
    typedef typename row_type::Iterator ColIterator;

    //! begin iterator
    Iterator begin ()
    {
      return Iterator(*this,0);
    }

    //! end iterator
    Iterator end ()
    {
      return Iterator(*this,n);
    }

    //! begin iterator
    Iterator rbegin ()
    {
      return Iterator(*this,n-1);
    }

    //! end iterator
    Iterator rend ()
    {
      return Iterator(*this,-1);
    }

    //! Iterator class for sequential access
    typedef FieldIterator<const FieldMatrix<K,n,m>,const row_type> ConstIterator;
    //! typedef for stl compliant access
    typedef ConstIterator const_iterator;
    //! rename the iterators for easier access
    typedef ConstIterator ConstRowIterator;
    //! rename the iterators for easier access
    typedef typename row_type::ConstIterator ConstColIterator;

    //! begin iterator
    ConstIterator begin () const
    {
      return ConstIterator(*this,0);
    }

    //! end iterator
    ConstIterator end () const
    {
      return ConstIterator(*this,n);
    }

    //! begin iterator
    ConstIterator rbegin () const
    {
      return ConstIterator(*this,n-1);
    }

    //! end iterator
    ConstIterator rend () const
    {
      return ConstIterator(*this,-1);
    }

    //===== assignment from scalar

    FieldMatrix& operator= (const K& k)
    {
      a[0] = k;
      return *this;
    }

    //===== vector space arithmetic

    //! vector space addition
    FieldMatrix& operator+= (const K& y)
    {
      a[0] += y;
      return *this;
    }

    //! vector space subtraction
    FieldMatrix& operator-= (const K& y)
    {
      a[0] -= y;
      return *this;
    }

    //! vector space multiplication with scalar
    FieldMatrix& operator*= (const K& k)
    {
      a[0] *= k;
      return *this;
    }

    //! vector space division by scalar
    FieldMatrix& operator/= (const K& k)
    {
      a[0] /= k;
      return *this;
    }

    //===== linear maps

    //! y += A x
    void umv (const FieldVector<K,1>& x, FieldVector<K,1>& y) const
    {
      y.p += a[0] * x.p;
    }

    //! y += A^T x
    void umtv (const FieldVector<K,1>& x, FieldVector<K,1>& y) const
    {
      y.p += a[0] * x.p;
    }

    //! y += A^H x
    void umhv (const FieldVector<K,1>& x, FieldVector<K,1>& y) const
    {
      y.p += fm_ck(a[0]) * x.p;
    }

    //! y -= A x
    void mmv (const FieldVector<K,1>& x, FieldVector<K,1>& y) const
    {
      y.p -= a[0] * x.p;
    }

    //! y -= A^T x
    void mmtv (const FieldVector<K,1>& x, FieldVector<K,1>& y) const
    {
      y.p -= a[0] * x.p;
    }

    //! y -= A^H x
    void mmhv (const FieldVector<K,1>& x, FieldVector<K,1>& y) const
    {
      y.p -= fm_ck(a[0]) * x.p;
    }

    //! y += alpha A x
    void usmv (const K& alpha, const FieldVector<K,1>& x, FieldVector<K,1>& y) const
    {
      y.p += alpha * a[0] * x.p;
    }

    //! y += alpha A^T x
    void usmtv (const K& alpha, const FieldVector<K,1>& x, FieldVector<K,1>& y) const
    {
      y.p += alpha * a[0] * x.p;
    }

    //! y += alpha A^H x
    void usmhv (const K& alpha, const FieldVector<K,1>& x, FieldVector<K,1>& y) const
    {
      y.p += alpha * fm_ck(a[0]) * x.p;
    }

    //===== norms

    //! frobenius norm: sqrt(sum over squared values of entries)
    double frobenius_norm () const
    {
      return sqrt(fvmeta_abs2(a[0]));
    }

    //! square of frobenius norm, need for block recursion
    double frobenius_norm2 () const
    {
      return fvmeta_abs2(a[0]);
    }

    //! infinity norm (row sum norm, how to generalize for blocks?)
    double infinity_norm () const
    {
      return std::abs(a[0]);
    }

    //! simplified infinity norm (uses Manhattan norm for complex values)
    double infinity_norm_real () const
    {
      return fvmeta_abs_real(a[0]);
    }

    //===== solve

    //! Solve system A x = b
    void solve (FieldVector<K,1>& x, const FieldVector<K,1>& b) const
    {
      x.p = b.p/a[0];
    }

    //! compute inverse
    void invert ()
    {
      a[0] = 1/a[0];
    }

    //! calculates the determinant of this matrix
    K determinant () const
    {
      return std::abs(a[0]);
    }

    //! left multiplication
    FieldMatrix& leftmultiply (const FieldMatrix& M)
    {
      a[0] *= M.a[0];
      return *this;
    }

    //! left multiplication
    FieldMatrix& rightmultiply (const FieldMatrix& M)
    {
      a[0] *= M.a[0];
      return *this;
    }


    //===== sizes

    //! number of blocks in row direction
    size_type N () const
    {
      return 1;
    }

    //! number of blocks in column direction
    size_type M () const
    {
      return 1;
    }

    //! row dimension of block r
    size_type rowdim (size_type r) const
    {
      return 1;
    }

    //! col dimension of block c
    size_type coldim (size_type c) const
    {
      return 1;
    }

    //! dimension of the destination vector space
    size_type rowdim () const
    {
      return 1;
    }

    //! dimension of the source vector space
    size_type coldim () const
    {
      return 1;
    }

    //===== query

    //! return true when (i,j) is in pattern
    bool exists (size_type i, size_type j) const
    {
      return i==0 && j==0;
    }

    //===== conversion operator

    operator K () const {return a[0];}

  private:
    // the data, just a single row with a single scalar
    row_type a;
  };

  namespace FMatrixHelp {


    //! invert scalar without changing the original matrix
    template <typename K>
    static inline K invertMatrix (const FieldMatrix<K,1,1> &matrix, FieldMatrix<K,1,1> &inverse)
    {
      inverse[0][0] = 1.0/matrix[0][0];
      return matrix[0][0];
    }

    //! invert scalar without changing the original matrix
    template <typename K>
    static inline K invertMatrix_retTransposed (const FieldMatrix<K,1,1> &matrix, FieldMatrix<K,1,1> &inverse)
    {
      return invertMatrix(matrix,inverse);
    }


    //! invert 2x2 Matrix without changing the original matrix
    template <typename K>
    static inline K invertMatrix (const FieldMatrix<K,2,2> &matrix, FieldMatrix<K,2,2> &inverse)
    {
      // code generated by maple
      K det = (matrix[0][0]*matrix[1][1] - matrix[0][1]*matrix[1][0]);
      K det_1 = 1.0/det;
      inverse[0][0] =   matrix[1][1] * det_1;
      inverse[0][1] = - matrix[0][1] * det_1;
      inverse[1][0] = - matrix[1][0] * det_1;
      inverse[1][1] =   matrix[0][0] * det_1;
      return det;
    }

    //! invert 2x2 Matrix without changing the original matrix
    //! return transposed matrix
    template <typename K>
    static inline K invertMatrix_retTransposed (const FieldMatrix<K,2,2> &matrix, FieldMatrix<K,2,2> &inverse)
    {
      // code generated by maple
      K det = (matrix[0][0]*matrix[1][1] - matrix[0][1]*matrix[1][0]);
      K det_1 = 1.0/det;
      inverse[0][0] =   matrix[1][1] * det_1;
      inverse[1][0] = - matrix[0][1] * det_1;
      inverse[0][1] = - matrix[1][0] * det_1;
      inverse[1][1] =   matrix[0][0] * det_1;
      return det;
    }

    //! invert 3x3 Matrix without changing the original matrix
    template <typename K>
    static inline K invertMatrix (const FieldMatrix<K,3,3> &matrix, FieldMatrix<K,3,3> &inverse)
    {
      // code generated by maple
      K t4  = matrix[0][0] * matrix[1][1];
      K t6  = matrix[0][0] * matrix[1][2];
      K t8  = matrix[0][1] * matrix[1][0];
      K t10 = matrix[0][2] * matrix[1][0];
      K t12 = matrix[0][1] * matrix[2][0];
      K t14 = matrix[0][2] * matrix[2][0];

      K det = (t4*matrix[2][2]-t6*matrix[2][1]-t8*matrix[2][2]+
               t10*matrix[2][1]+t12*matrix[1][2]-t14*matrix[1][1]);
      K t17 = 1.0/det;

      inverse[0][0] =  (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1])*t17;
      inverse[0][1] = -(matrix[0][1] * matrix[2][2] - matrix[0][2] * matrix[2][1])*t17;
      inverse[0][2] =  (matrix[0][1] * matrix[1][2] - matrix[0][2] * matrix[1][1])*t17;
      inverse[1][0] = -(matrix[1][0] * matrix[2][2] - matrix[1][2] * matrix[2][0])*t17;
      inverse[1][1] =  (matrix[0][0] * matrix[2][2] - t14) * t17;
      inverse[1][2] = -(t6-t10) * t17;
      inverse[2][0] =  (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0]) * t17;
      inverse[2][1] = -(matrix[0][0] * matrix[2][1] - t12) * t17;
      inverse[2][2] =  (t4-t8) * t17;

      return det;
    }

    //! invert 3x3 Matrix without changing the original matrix
    template <typename K>
    static inline K invertMatrix_retTransposed (const FieldMatrix<K,3,3> &matrix, FieldMatrix<K,3,3> &inverse)
    {
      // code generated by maple
      K t4  = matrix[0][0] * matrix[1][1];
      K t6  = matrix[0][0] * matrix[1][2];
      K t8  = matrix[0][1] * matrix[1][0];
      K t10 = matrix[0][2] * matrix[1][0];
      K t12 = matrix[0][1] * matrix[2][0];
      K t14 = matrix[0][2] * matrix[2][0];

      K det = (t4*matrix[2][2]-t6*matrix[2][1]-t8*matrix[2][2]+
               t10*matrix[2][1]+t12*matrix[1][2]-t14*matrix[1][1]);
      K t17 = 1.0/det;

      inverse[0][0] =  (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1])*t17;
      inverse[1][0] = -(matrix[0][1] * matrix[2][2] - matrix[0][2] * matrix[2][1])*t17;
      inverse[2][0] =  (matrix[0][1] * matrix[1][2] - matrix[0][2] * matrix[1][1])*t17;
      inverse[0][1] = -(matrix[1][0] * matrix[2][2] - matrix[1][2] * matrix[2][0])*t17;
      inverse[1][1] =  (matrix[0][0] * matrix[2][2] - t14) * t17;
      inverse[2][1] = -(t6-t10) * t17;
      inverse[0][2] =  (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0]) * t17;
      inverse[1][2] = -(matrix[0][0] * matrix[2][1] - t12) * t17;
      inverse[2][2] =  (t4-t8) * t17;

      return det;
    }

    //! calculates ret= A_t*A
    template <typename K, int rows,int cols>
    static inline void multTransposedMatrix(const FieldMatrix<K,rows,cols> &matrix, FieldMatrix<K,cols,cols>& ret)
    {
      typedef typename FieldMatrix<K,rows,cols>::size_type size_type;

      for(size_type i=0; i<cols; i++)
        for(size_type j=0; j<cols; j++)
        { ret[i][j]=0.0;
          for(size_type k=0; k<rows; k++)
            ret[i][j]+=matrix[k][i]*matrix[k][j];}
    }

    //! calculates ret = matrix * x
    template <typename K, int dim>
    static inline void multAssign(const FieldMatrix<K,dim,dim> &matrix, const FieldVector<K,dim> & x, FieldVector<K,dim> & ret)
    {
      typedef typename FieldMatrix<K,dim,dim>::size_type size_type;

      for(size_type i=0; i<dim; i++)
      {
        ret[i] = 0.0;
        for(size_type j=0; j<dim; j++)
        {
          ret[i] += matrix[i][j]*x[j];
        }
      }
    }

    //! calculates ret = matrix * x
    template <typename K, int rows,int cols>
    static inline void multAssign(const FieldMatrix<K,rows,cols> &matrix, const FieldVector<K,cols> & x, FieldVector<K,rows> & ret)
    {
      typedef typename FieldMatrix<K,rows,cols>::size_type size_type;

      for(size_type i=0; i<rows; i++)
      {
        ret[i] = 0.0;
        for(size_type j=0; j<cols; j++)
        {
          ret[i] += matrix[i][j]*x[j];
        }
      }
    }

    //! calculates ret = matrix * x
    template <typename K, int dim>
    static inline void multAssignTransposed( const FieldMatrix<K,dim,dim> &matrix, const FieldVector<K,dim> & x, FieldVector<K,dim> & ret)
    {
      typedef typename FieldMatrix<K,dim,dim>::size_type size_type;

      for(size_type i=0; i<dim; i++)
      {
        ret[i] = 0.0;
        for(size_type j=0; j<dim; j++)
        {
          ret[i] += matrix[j][i]*x[j];
        }
      }
    }

    //! calculates ret = matrix * x
    template <typename K, int dim>
    static inline FieldVector<K,dim> mult(const FieldMatrix<K,dim,dim> &matrix, const FieldVector<K,dim> & x)
    {
      FieldVector<K,dim> ret;
      multAssign(matrix,x,ret);
      return ret;
    }



    //! calculates ret = matrix^T * x
    template <typename K, int rows, int cols>
    static inline FieldVector<K,cols> multTransposed(const FieldMatrix<K,rows,cols> &matrix, const FieldVector<K,rows> & x)
    {
      FieldVector<K,cols> ret;
      typedef typename FieldMatrix<K,rows,cols>::size_type size_type;
      for(size_type i=0; i<cols; i++)
      {
        ret[i] = 0.0;
        for(size_type j=0; j<rows; j++)
        {
          ret[i] += matrix[j][i]*x[j];
        }
      }
      return ret;
    }

  } // end namespace FMatrixHelp

#ifdef DUNE_EXPRESSIONTEMPLATES
  template <class K, int N, int M>
  struct BlockType< FieldMatrix<K,N,M> >
  {
    typedef K type;
  };

  template <class K, int N, int M>
  struct FieldType< FieldMatrix<K,N,M> >
  {
    typedef K type;
  };
#endif // DUNE_EXPRESSIONTEMPLATES

  /** @} end documentation */

} // end namespace

#endif
