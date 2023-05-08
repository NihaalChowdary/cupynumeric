/* Copyright 2021 NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "cunumeric/runtime.h"

#include "cunumeric/ndarray.h"

namespace cunumeric {

/*static*/ CuNumericRuntime* CuNumericRuntime::runtime_;

static std::map<std::pair<UnaryRedCode, legate::Type::Code>, Scalar> identities;

extern void bootstrapping_callback(Legion::Machine machine,
                                   Legion::Runtime* runtime,
                                   const std::set<Legion::Processor>& local_procs);

void initialize(int32_t argc, char** argv)
{
  Legion::Runtime::perform_registration_callback(bootstrapping_callback, true /*global*/);
}

CuNumericRuntime::CuNumericRuntime(legate::Runtime* legate_runtime, legate::LibraryContext* context)
  : legate_runtime_(legate_runtime), context_(context)
{
}

NDArray CuNumericRuntime::create_array(std::unique_ptr<legate::Type> type)
{
  auto store = legate_runtime_->create_store(std::move(type));
  return NDArray(std::move(store));
}

NDArray CuNumericRuntime::create_array(const legate::Type& type)
{
  return create_array(type.clone());
}

NDArray CuNumericRuntime::create_array(std::vector<size_t> shape,
                                       std::unique_ptr<legate::Type> type)
{
  auto store = legate_runtime_->create_store(shape, std::move(type), true /*optimize_scalar*/);
  return NDArray(std::move(store));
}

NDArray CuNumericRuntime::create_array(std::vector<size_t> shape, const legate::Type& type)
{
  return create_array(std::move(shape), type.clone());
}

legate::LogicalStore CuNumericRuntime::create_scalar_store(const Scalar& value)
{
  return legate_runtime_->create_store(value);
}

struct generate_identity_fn {
  template <UnaryRedCode OP>
  struct generator {
    template <legate::Type::Code CODE, std::enable_if_t<UnaryRedOp<OP, CODE>::valid>* = nullptr>
    Scalar operator()()
    {
      auto value = UnaryRedOp<OP, CODE>::OP::identity;
      return Scalar(value);
    }

    template <legate::Type::Code CODE, std::enable_if_t<!UnaryRedOp<OP, CODE>::valid>* = nullptr>
    Scalar operator()()
    {
      assert(false);
      return Scalar();
    }
  };

  template <UnaryRedCode OP>
  Scalar operator()(legate::Type::Code code)
  {
    return legate::type_dispatch(code, generator<OP>{});
  }
};

Scalar CuNumericRuntime::get_reduction_identity(UnaryRedCode op, const legate::Type& type)
{
  auto key    = std::make_pair(op, type.code);
  auto finder = identities.find(key);
  if (identities.end() != finder) return finder->second;

  auto identity   = op_dispatch(op, generate_identity_fn{}, type.code);
  identities[key] = identity;
  return identity;
}

namespace {

const std::unordered_map<UnaryRedCode, legate::ReductionOpKind> TO_CORE_REDOP = {
  {UnaryRedCode::ALL, legate::ReductionOpKind::MUL},
  {UnaryRedCode::ANY, legate::ReductionOpKind::ADD},
  {UnaryRedCode::ARGMAX, legate::ReductionOpKind::MAX},
  {UnaryRedCode::ARGMIN, legate::ReductionOpKind::MIN},
  {UnaryRedCode::CONTAINS, legate::ReductionOpKind::ADD},
  {UnaryRedCode::COUNT_NONZERO, legate::ReductionOpKind::ADD},
  {UnaryRedCode::MAX, legate::ReductionOpKind::MAX},
  {UnaryRedCode::MIN, legate::ReductionOpKind::MIN},
  {UnaryRedCode::PROD, legate::ReductionOpKind::MUL},
  {UnaryRedCode::SUM, legate::ReductionOpKind::ADD},
};

}  // namespace

Legion::ReductionOpID CuNumericRuntime::get_reduction_op(UnaryRedCode op, const legate::Type& type)
{
  return type.find_reduction_operator(TO_CORE_REDOP.at(op));
}

std::unique_ptr<legate::AutoTask> CuNumericRuntime::create_task(CuNumericOpCode op_code)
{
  return legate_runtime_->create_task(context_, op_code);
}

void CuNumericRuntime::submit(std::unique_ptr<legate::Task> task)
{
  legate_runtime_->submit(std::move(task));
}

uint32_t CuNumericRuntime::get_next_random_epoch() { return next_epoch_++; }

/*static*/ CuNumericRuntime* CuNumericRuntime::get_runtime() { return runtime_; }

/*static*/ void CuNumericRuntime::initialize(legate::Runtime* legate_runtime,
                                             legate::LibraryContext* context)
{
  runtime_ = new CuNumericRuntime(legate_runtime, context);
}

}  // namespace cunumeric
