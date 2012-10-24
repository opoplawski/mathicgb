#include "stdinc.h"
#include "SparseMatrix.hpp"

#include "Poly.hpp"
#include <algorithm>

std::ostream& operator<<(std::ostream& out, const SparseMatrix& matrix) {
  matrix.print(out);
  return out;
}

void SparseMatrix::rowToPolynomial
(RowIndex row, std::vector<monomial> colMonomials, Poly& poly) {
  MATHICGB_ASSERT(colMonomials.size() == colCount());
  poly.setToZero();
  auto end = rowEnd(row);
  poly.reserve(entryCountInRow(row));
  for (auto it = rowBegin(row); it != end; ++it) {
    MATHICGB_ASSERT(it.index() < colMonomials.size());
    if (it.scalar() != 0)
      poly.appendTerm(it.scalar(), colMonomials[it.index()]);
  }
}

void SparseMatrix::sortRowsByIncreasingPivots() {
  SparseMatrix ordered;

  // compute pairs (pivot column index, row)
  std::vector<std::pair<SparseMatrix::ColIndex, SparseMatrix::RowIndex> > order;
  const SparseMatrix::RowIndex lRowCount = rowCount();
  const SparseMatrix::ColIndex lColCount = colCount();
  for (SparseMatrix::RowIndex row = 0; row < lRowCount; ++row) {
    if (entryCountInRow(row) == 0)
      order.push_back(std::make_pair(lColCount, row));
    else
      order.push_back(std::make_pair(rowBegin(row).index(), row));
  }

  // sort pairs by pivot column index
  std::sort(order.begin(), order.end());

  // construct ordered with pivot columns in increaing order
  ordered.clear(lColCount);
  for (size_t i = 0; i < lRowCount; ++i) {
    const SparseMatrix::RowIndex row = order[i].second;
    auto it = rowBegin(row);
    const auto end = rowEnd(row);
    for (; it != end; ++it)
      ordered.appendEntry(it.index(), it.scalar());
    ordered.rowDone();
  }

  *this = std::move(ordered);
}

void SparseMatrix::applyColumnMap(std::vector<ColIndex> colMap) {
  MATHICGB_ASSERT(colMap.size() >= colCount());
  const auto end = mColIndices.end();
  for (auto it = mColIndices.begin(); it != end; ++it) {
    MATHICGB_ASSERT(*it < colCount());
    *it = colMap[*it];
  }
}

void SparseMatrix::print(std::ostream& out) const {
  if (rowCount() == 0)
    out << "matrix with no rows\n";
  for (RowIndex row = 0; row < rowCount(); ++row) {
    out << row << ':';
    const auto end = rowEnd(row);
    for (auto it = rowBegin(row); it != end; ++it) {
      MATHICGB_ASSERT(it.index() < colCount());
      out << ' ' << it.index() << '#' << it.scalar();
    }
    out << '\n';
  }
}

std::string SparseMatrix::toString() const {
  std::ostringstream out;
  print(out);
  return out.str();
}

void SparseMatrix::appendRowAndNormalize(const SparseMatrix& matrix, RowIndex row, Scalar modulus) {
  MATHICGB_ASSERT(row < matrix.rowCount());
  auto it = matrix.rowBegin(row);
  const auto end = matrix.rowEnd(row);
  if (it != end) {
    appendEntry(it.index(), 1);
    Scalar lead = it.scalar();
    ++it;
    if (it != end) {
      Scalar inverse = modularInverse(lead, modulus);
      do {
        uint32 prod = static_cast<uint32>(inverse) * it.scalar();
        uint16 prodMod = static_cast<uint16>(prod % modulus);
        appendEntry(it.index(), prodMod);
        ++it;
      } while (it != end);
    }
  }
  rowDone();
}

void SparseMatrix::appendRow(const SparseMatrix& matrix, RowIndex row) {
  MATHICGB_ASSERT(row < matrix.rowCount());
  auto it = matrix.rowBegin(row);
  const auto end = matrix.rowEnd(row);
  for (; it != end; ++it)
    appendEntry(it.index(), it.scalar());
  rowDone();
}
  
void SparseMatrix::swap(SparseMatrix& matrix) {
  std::swap(mColIndices, matrix.mColIndices);
  std::swap(mEntries, matrix.mEntries);
  std::swap(mRows, matrix.mRows);
  std::swap(mColCount, matrix.mColCount);
}
  
void SparseMatrix::clear(ColIndex newColCount) {
  mColIndices.clear();
  mEntries.clear();
  mRows.clear();
  mColCount = newColCount;
}

void SparseMatrix::appendRowWithModulus(std::vector<uint64> const& v, Scalar modulus) {
  MATHICGB_ASSERT(v.size() == colCount());
  ColIndex count = colCount();
  for (ColIndex col = 0; col < count; ++col) {
    Scalar scalar = static_cast<Scalar>(v[col] % modulus);
    if (scalar != 0)
      appendEntry(col, scalar);
  }
  rowDone();
}

void SparseMatrix::appendRow(std::vector<uint64> const& v, ColIndex leadCol) {
  MATHICGB_ASSERT(v.size() == colCount());
#ifdef MATHICGB_DEBUG
  for (ColIndex col = leadCol; col < leadCol; ++col) {
    MATHICGB_ASSERT(v[col] == 0);
  }
#endif

  ColIndex count = colCount();
  for (ColIndex col = leadCol; col < count; ++col) {
	MATHICGB_ASSERT(v[col] < std::numeric_limits<Scalar>::max());
    if (v[col] != 0)
      appendEntry(col, static_cast<Scalar>(v[col]));
  }
  rowDone();
}

void SparseMatrix::appendRowWithModulusNormalized(std::vector<uint64> const& v, Scalar modulus) {
  MATHICGB_ASSERT(v.size() == colCount());
  ColIndex count = colCount();
  uint16 multiply = 1;
   
  bool first = true;
  for (ColIndex col = 0; col < count; ++col) {
    Scalar scalar = static_cast<Scalar>(v[col] % modulus);
    if (scalar == 0)
      continue;
    if (first) {
      multiply = modularInverse(scalar, modulus);
      scalar = 1;
      first = false;
    } else {
      uint32 prod = static_cast<uint32>(multiply) * scalar;
      scalar = prod % modulus;
    }
    appendEntry(col, scalar);
  }
  rowDone();
}

bool SparseMatrix::appendRowWithModulusIfNonZero(std::vector<uint64> const& v, Scalar modulus) {
  appendRowWithModulus(v, modulus);
  MATHICGB_ASSERT(rowCount() > 0);
  if (mRows.back().empty()) {
    mRows.pop_back();
    return false;
  } else
    return true;
}

void SparseMatrix::trimLeadingZeroColumns(ColIndex trimThisMany) {
  MATHICGB_ASSERT(trimThisMany <= colCount());
  const auto end = mColIndices.end();
  for (auto it = mColIndices.begin(); it != end; ++it) {
    MATHICGB_ASSERT(*it >= trimThisMany);
    *it -= trimThisMany;
  }
  mColCount -= trimThisMany;
}

void SparseMatrix::reserveEntries(size_t count) {
  if (count < mEntries.capacity())
    return;

  ptrdiff_t scalarPtrDelta;
  {
    const auto begin = new Scalar[count];
    const auto capacityEnd = begin + count;
    scalarPtrDelta = begin - mEntries.begin();
    delete[] mEntries.setMemoryAndCopy(begin, capacityEnd);
  }

  ptrdiff_t entryPtrDelta;
  {
    const auto begin = new ColIndex[count];
    const auto capacityEnd = begin + count;
    entryPtrDelta = begin - mColIndices.begin();
    delete[] mColIndices.setMemoryAndCopy(begin, capacityEnd);
  }

  const auto rowEnd = mRows.end();
  for (auto it = mRows.begin(); it != rowEnd; ++it) {
    it->mIndicesBegin += entryPtrDelta;
    it->mIndicesEnd += entryPtrDelta;
    it->mScalarsBegin += scalarPtrDelta;
    it->mScalarsEnd += scalarPtrDelta;
  }
}

void SparseMatrix::growEntryCapacity() {
  MATHICGB_ASSERT(mColIndices.size() == mEntries.size());
  MATHICGB_ASSERT(mColIndices.capacity() == mEntries.capacity());

  const size_t initialCapacity = 1 << 16;
  const size_t growthFactor = 2;
  const size_t newCapacity =
    mEntries.empty() ? initialCapacity : mEntries.capacity() * growthFactor;
  reserveEntries(newCapacity);

  MATHICGB_ASSERT(mColIndices.size() == mEntries.size());
  MATHICGB_ASSERT(mColIndices.capacity() == newCapacity);
  MATHICGB_ASSERT(mEntries.capacity() == newCapacity);
}
