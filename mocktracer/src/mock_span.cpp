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

MockSpan::~MockSpan() {
  if (!is_finished_) {
    Finish();
  }
}

void MockSpan::FinishWithOptions(const FinishSpanOptions& options) noexcept {
  // Ensure the span is only finished once.
  if (is_finished_.exchange(true)) {
    return;
  }

  auto finish_timestamp = options.finish_steady_timestamp;
  if (finish_timestamp == SteadyTime{}) {
    finish_timestamp = SteadyClock::now();
  }

  data_.duration = finish_timestamp - start_steady_;

  span_context_.SetData(data_.span_context);

  recorder_.RecordSpan(std::move(data_));
}

void MockSpan::SetOperationName(string_view name) noexcept try {
  std::lock_guard<std::mutex> lock_guard{mutex_};
  data_.operation_name = name;
} catch (const std::exception&) {
  // Ignore operation
}

void MockSpan::SetTag(string_view key,
              const opentracing::Value& value) noexcept try {
  std::lock_guard<std::mutex> lock_guard{mutex_};
  data_.tags[key] = value;
} catch (const std::exception&) {
  // Ignore upon error.
}

void MockSpan::SetBaggageItem(string_view restricted_key,
                      string_view value) noexcept try {
  std::lock_guard<std::mutex> lock_guard{span_context_.baggage_mutex_};
  span_context_.data_.baggage.emplace(restricted_key, value);
} catch (const std::exception&) {
  // Drop baggage item upon error.
}

std::string MockSpan::BaggageItem(string_view restricted_key) const noexcept try {
  std::lock_guard<std::mutex> lock_guard{span_context_.baggage_mutex_};
  auto lookup = span_context_.data_.baggage.find(restricted_key);
  if (lookup != span_context_.data_.baggage.end()) {
    return lookup->second;
  }
  return {};
} catch(const std::exception&) {
  // Return empty string upon error.
  return {};
}

void MockSpan::Log(std::initializer_list<std::pair<string_view, Value>>
                       fields) noexcept {}

}  // namespace mocktracer
END_OPENTRACING_ABI_NAMESPACE
}  // namespace opentracing
