/**
 * This is the C++ adaptation and derivative work of Myia (https://github.com/mila-iqia/myia/).
 *
 * Copyright 2019-2021 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MINDSPORE_CORE_ABSTRACT_DSHAPE_H_
#define MINDSPORE_CORE_ABSTRACT_DSHAPE_H_

#include <vector>
#include <string>
#include <sstream>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <algorithm>

#include "utils/log_adapter.h"
#include "base/base.h"
#include "utils/shape_utils.h"

namespace mindspore {
namespace abstract {
class BaseShape;
using BaseShapePtr = std::shared_ptr<BaseShape>;
using BaseShapePtrList = std::vector<BaseShapePtr>;

/// \brief BaseShape defines the basic virtual class of NoShape and Shape classes.
class MS_CORE_API BaseShape : public Base {
 public:
  /// \brief Constructor of BaseShape.
  BaseShape() = default;

  /// \brief Destructor of BaseShape.
  ~BaseShape() override = default;

  MS_DECLARE_PARENT(BaseShape, Base)

  /// \brief Check whether 2 objects are equal.
  ///
  /// \param[in] other Another object.
  /// \return True if current object is equal to another, otherwise false.
  virtual bool operator==(const BaseShape &other) const;

  /// \brief Check whether 2 objects are not equal.
  ///
  /// \param[in] other Another object.
  /// \return True if current object is not equal to another, otherwise false.
  bool operator!=(const BaseShape &other) const;

  /// \brief Calculate the hash value of BaseShape.
  ///
  /// \return The hash value of BaseShape.
  std::size_t hash() const override { return tid(); }

  /// \brief Whether the object's dimensions are dynamic.
  ///
  /// \return True if the object's dimensions are dynamic, otherwise false.
  virtual bool IsDynamic() const = 0;

  /// \brief Whether the object's dimensions are unknown.
  ///
  /// \return True if the object's dimensions are unknown, otherwise false.
  virtual bool IsDimUnknown() const = 0;

  /// \brief Clone a new object by this one.
  ///
  /// \return New cloned object.
  virtual BaseShapePtr Clone() const = 0;

  /// \brief Broaden the shape.
  virtual void Broaden() {}
};

/// \brief NoShape defines an invalid shape.
class MS_CORE_API NoShape final : public BaseShape {
 public:
  MS_DECLARE_PARENT(NoShape, BaseShape)

  BaseShapePtr Clone() const override { return std::make_shared<NoShape>(); }

  /// \brief Get the description string about the NoShape object.
  ///
  /// \return The description string about the NoShape object.
  std::string ToString() const override { return type_name(); }

  bool IsDynamic() const override { return false; }

  bool IsDimUnknown() const override { return false; }
};

inline const std::shared_ptr<NoShape> kNoShape = std::make_shared<NoShape>();

/// \brief Shape defines dimensions of tensor.
class MS_CORE_API Shape final : public BaseShape {
 public:
  static const int64_t SHP_ANY = -1;

  /// \brief Constructor of Shape.
  Shape() : shape_() {}

  /// \brief Constructor of Shape.
  ///
  /// \param[in] list Initial shape dimensions.
  Shape(const std::initializer_list<int64_t> &list) : shape_(list) {}

  /// \brief Constructor of Shape.
  ///
  /// \param[in] list Initial shape dimensions.
  explicit Shape(const ShapeVector &list) : shape_(list) {}

  /// \brief Constructor of Shape.
  ///
  /// \param[in] list Initial shape dimensions.
  /// \param[in] min_shape Minimum shape dimensions of dynamic shape.
  /// \param[in] max_shape Maximum shape dimensions of dynamic shape.
  Shape(const ShapeVector &list, const ShapeVector &min_shape, const ShapeVector &max_shape)
      : shape_(list), min_shape_(min_shape), max_shape_(max_shape) {}

  /// \brief Destructor of Shape.
  ~Shape() override = default;
  MS_DECLARE_PARENT(Shape, BaseShape)

  /// \brief Get the description string about the Shape object.
  ///
  /// \return The description string about the Shape object.
  std::string ToString() const override;

  /// \brief Get the debug information about the Shape object.
  ///
  /// \return The debug information about the Shape object.
  std::string DumpText() const override;

  bool operator==(const BaseShape &other) const override;

  BaseShapePtr Clone() const override { return std::make_shared<Shape>(shape_, min_shape_, max_shape_); }

  void Broaden() override;

  /// \brief Set shape dimensions of Shape object.
  ///
  /// \param[in] shape Dimensions of shape.
  void set_shape(const ShapeVector &shape) { shape_ = shape; }

  /// \brief Get shape dimensions.
  ///
  /// \return Shape dimensions.
  const ShapeVector &shape() { return shape_; }

  /// \brief Get minimum shape dimensions.
  ///
  /// \return Minimum shape dimensions.
  const ShapeVector &min_shape() { return min_shape_; }

  /// \brief Get maximum shape dimensions.
  ///
  /// \return Maximum shape dimensions.
  const ShapeVector &max_shape() { return max_shape_; }

  bool IsDynamic() const override {
    return std::any_of(shape_.begin(), shape_.end(), [](int64_t s) { return s < 0; });
  }

  bool IsDimUnknown() const override {
    return std::any_of(shape_.begin(), shape_.end(), [](int64_t s) { return s < -1; });
  }

 private:
  ShapeVector shape_;      // use SHP_ANY to implement the any shape in python
  ShapeVector min_shape_;  // record minimum length for each dynamic dimension
  ShapeVector max_shape_;  // record maximum length for each dynamic dimension
};
using ShapePtr = std::shared_ptr<Shape>;
using ShapePtrList = std::vector<ShapePtr>;

/// \brief SequequeShape defines base class of multiple-shape classes.
class MS_CORE_API SequeueShape : public BaseShape {
 public:
  /// \brief Constructor of SequeueShape.
  SequeueShape() : p_shapes_() {}

  /// \brief Constructor of SequeueShape.
  ///
  /// \param[in]  shapes All element-shapes.
  explicit SequeueShape(const BaseShapePtrList &shapes) : p_shapes_(shapes) {}

  /// \brief Destructor of SequeueShape.
  ~SequeueShape() override = default;
  MS_DECLARE_PARENT(SequeueShape, BaseShape)

  /// \brief Get the description string about the SequeueShape object.
  ///
  /// \return The description string about the SequeueShape object.
  std::string ToString() const override;

  /// \brief Clone all element-shapes.
  ///
  /// \return New cloned element-shapes.
  BaseShapePtrList ElementsClone() const;

  /// \brief Check whether SequeueShape object is equal to a BaseShape object.
  ///
  /// \param[in] other Another SequeueShape object.
  /// \return True if current SequeueShape object is equal to another BaseShape object, otherwise false.
  template <typename T>
  bool SequeueEqual(const BaseShape &other) const;

  /// \brief Get all element-shapes.
  ///
  /// \return  All element-shapes.
  const BaseShapePtrList &shape() const { return p_shapes_; }

  /// \brief Get the number of element-shapes.
  ///
  /// \return The number of element-shapes.
  size_t size() const { return p_shapes_.size(); }

  /// \brief Get the element-shape by index through operator '[]'.
  ///
  /// \param[in] dim The index of element shape.
  /// \return The element shape got by index.
  const BaseShapePtr operator[](std::size_t dim) const { return p_shapes_[dim]; }

  bool IsDynamic() const override {
    return std::any_of(p_shapes_.begin(), p_shapes_.end(), [](const BaseShapePtr &bs) { return bs->IsDynamic(); });
  }

  bool IsDimUnknown() const override {
    return std::any_of(p_shapes_.begin(), p_shapes_.end(), [](const BaseShapePtr &bs) { return bs->IsDimUnknown(); });
  }

 protected:
  BaseShapePtrList p_shapes_;  // shape list of each elements
};
using SequeueShapePtr = std::shared_ptr<SequeueShape>;

/// \brief TupleShape defines shape used by tuple with tensor inside.
class MS_CORE_API TupleShape final : public SequeueShape {
 public:
  /// \brief Constructor of TupleShape.
  TupleShape() : SequeueShape() {}

  /// \brief Constructor of TupleShape.
  ///
  /// \param[in] shapes Element-shapes of TupleShape.
  explicit TupleShape(const BaseShapePtrList &shapes) : SequeueShape(shapes) {}

  /// \brief Destructor of TupleShape.
  ~TupleShape() override = default;
  MS_DECLARE_PARENT(TupleShape, SequeueShape)

  std::string ToString() const override { return type_name() + "(" + SequeueShape::ToString() + ")"; }

  BaseShapePtr Clone() const override { return std::make_shared<TupleShape>(ElementsClone()); }

  bool operator==(const BaseShape &other) const override { return SequeueEqual<TupleShape>(other); }
};
using TupleShapePtr = std::shared_ptr<TupleShape>;

/// \brief ListShape defines shape used by list with tensor inside.
class MS_CORE_API ListShape final : public SequeueShape {
 public:
  /// \brief Constructor of ListShape.
  ListShape() : SequeueShape() {}
  /// \brief Constructor of ListShape.
  ///
  /// \param[in] shapes Element-shapes of ListShape.
  explicit ListShape(const BaseShapePtrList &shapes) : SequeueShape(shapes) {}

  /// \brief Destructor of ListShape.
  ~ListShape() override = default;
  MS_DECLARE_PARENT(ListShape, SequeueShape)

  std::string ToString() const override { return type_name() + "[" + SequeueShape::ToString() + "]"; }

  BaseShapePtr Clone() const override { return std::make_shared<ListShape>(SequeueShape::ElementsClone()); }

  bool operator==(const BaseShape &other) const override { return SequeueEqual<ListShape>(other); }
};
using ListShapePtr = std::shared_ptr<ListShape>;
}  // namespace abstract
}  // namespace mindspore

#endif  // MINDSPORE_CORE_ABSTRACT_DSHAPE_H_
