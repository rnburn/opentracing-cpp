#include "mock_span.h"
#include <random>

namespace opentracing {
BEGIN_OPENTRACING_ABI_NAMESPACE
namespace mocktracer {

static uint64_t GenerateId() {
  static thread_local std::mt19937_64 rand_source{std::random_device()()};
  return rand_source();
}

static std::tuple<SystemTime, SteadyTime> ComputeStartTimestamps(
    const SystemTime& start_system_timestamp,
    const SteadyTime& start_steady_timestamp) {
  // If neither the system nor steady timestamps are set, get the tme from the
  // respective clocks; otherwise, use the set timestamp to initialize the
  // other.
  if (start_system_timestamp == SystemTime() &&
      start_steady_timestamp == SteadyTime()) {
    return std::tuple<SystemTime, SteadyTime>{SystemClock::now(),
                                              SteadyClock::now()};
  }
  if (start_system_timestamp == SystemTime()) {
    return std::tuple<SystemTime, SteadyTime>{
        opentracing::convert_time_point<SystemClock>(start_steady_timestamp),
        start_steady_timestamp};
  }
  if (start_steady_timestamp == SteadyTime()) {
    return std::tuple<SystemTime, SteadyTime>{
        start_system_timestamp,
        opentracing::convert_time_point<SteadyClock>(start_system_timestamp)};
  }
  return std::tuple<SystemTime, SteadyTime>{start_system_timestamp,
                                            start_steady_timestamp};
}

static bool SetSpanReference(
    const std::pair<SpanReferenceType, const SpanContext*>& reference,
    std::unordered_map<std::string, std::string>& baggage,
    SpanReferenceData& reference_data) {
  reference_data.reference_type = reference.first;
  if (reference.second == nullptr) {
    return false;
  }
  auto referenced_context =
      dynamic_cast<const MockSpanContext*>(reference.second);
  if (referenced_context == nullptr) {
    return false;
  }
  reference_data.trace_id = referenced_context->trace_id();
  reference_data.span_id = referenced_context->span_id();

  referenced_context->ForeachBaggageItem(
      [&baggage](const std::string& key, const std::string& value) {
        baggage[key] = value;
        return true;
      });

  return true;
}

MockSpan::MockSpan(std::shared_ptr<const Tracer>&& tracer, Recorder& recorder,
                   string_view operation_name, const StartSpanOptions& options)
    : tracer_{std::move(tracer)}, recorder_{recorder} {
  data_.operation_name = operation_name;

  // Set start timestamps
  std::tie(data_.start_timestamp, start_steady_) = ComputeStartTimestamps(
      options.start_system_timestamp, options.start_steady_timestamp);

  // Set references
  SpanContextData span_context_data;
  for (auto& reference : options.references) {
    SpanReferenceData reference_data;
    if (!SetSpanReference(reference, span_context_data.baggage,
                          reference_data)) {
      continue;
    }
    data_.references.push_back(reference_data);
  }

  // Set tags
  for (auto& tag : options.tags) {
    data_.tags[tag.first] = tag.second;
  }

  // Set span context
  span_context_data.trace_id =
      data_.references.empty() ? GenerateId() : data_.references[0].trace_id;
  span_context_data.span_id = GenerateId();
  span_context_ = MockSpanContext{std::move(span_context_data)};
}

}  // namespace mocktracer
END_OPENTRACING_ABI_NAMESPACE
}  // namespace opentracing
