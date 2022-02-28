/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "watchman/Errors.h"
#include "watchman/query/FileResult.h"
#include "watchman/query/Query.h"
#include "watchman/query/QueryExpr.h"
#include "watchman/query/TermRegistry.h"

#include <memory>

using namespace watchman;

namespace watchman {
class QueryContextBase;
}

enum since_what { SINCE_OCLOCK, SINCE_CCLOCK, SINCE_MTIME, SINCE_CTIME };

static struct {
  enum since_what value;
  const char* label;
} allowed_fields[] = {
    {since_what::SINCE_OCLOCK, "oclock"},
    {since_what::SINCE_CCLOCK, "cclock"},
    {since_what::SINCE_MTIME, "mtime"},
    {since_what::SINCE_CTIME, "ctime"},
};

class SinceExpr : public QueryExpr {
  std::unique_ptr<ClockSpec> spec;
  enum since_what field;

 public:
  explicit SinceExpr(std::unique_ptr<ClockSpec> spec, enum since_what field)
      : spec(std::move(spec)), field(field) {}

  EvaluateResult evaluate(QueryContextBase* ctx, FileResult* file) override {
    time_t tval = 0;

    auto since = spec->evaluate(
        ctx->clockAtStartOfQuery.position(),
        ctx->lastAgeOutTickValueAtStartOfQuery);

    // Note that we use >= for the time comparisons in here so that we
    // report the things that changed inclusive of the boundary presented.
    // This is especially important for clients using the coarse unix
    // timestamp as the since basis, as they would be much more
    // likely to miss out on changes if we didn't.

    switch (field) {
      case since_what::SINCE_OCLOCK:
      case since_what::SINCE_CCLOCK: {
        const auto clock =
            (field == since_what::SINCE_OCLOCK) ? file->otime() : file->ctime();
        if (!clock.has_value()) {
          return std::nullopt;
        }

        if (since.is_timestamp) {
          return clock->timestamp >= since.timestamp;
        }
        if (since.clock.is_fresh_instance) {
          return file->exists();
        }
        return clock->ticks > since.clock.ticks;
      }
      case since_what::SINCE_MTIME: {
        auto stat = file->stat();
        if (!stat.has_value()) {
          return std::nullopt;
        }
        tval = stat->mtime.tv_sec;
        break;
      }
      case since_what::SINCE_CTIME: {
        auto stat = file->stat();
        if (!stat.has_value()) {
          return std::nullopt;
        }
        tval = stat->ctime.tv_sec;
        break;
      }
    }

    assert(since.is_timestamp);
    return tval >= since.timestamp;
  }

  static std::unique_ptr<QueryExpr> parse(Query*, const json_ref& term) {
    auto selected_field = since_what::SINCE_OCLOCK;
    const char* fieldname = "oclock";

    if (!term.isArray()) {
      throw QueryParseError("\"since\" term must be an array");
    }

    if (json_array_size(term) < 2 || json_array_size(term) > 3) {
      throw QueryParseError("\"since\" term has invalid number of parameters");
    }

    const auto& jval = term.at(1);
    auto spec = ClockSpec::parseOptionalClockSpec(jval);
    if (!spec) {
      throw QueryParseError("invalid clockspec for \"since\" term");
    }
    if (std::holds_alternative<ClockSpec::NamedCursor>(spec->spec)) {
      throw QueryParseError("named cursors are not allowed in \"since\" terms");
    }

    if (term.array().size() == 3) {
      const auto& field = term.at(2);
      size_t i;
      bool valid = false;

      fieldname = json_string_value(field);
      if (!fieldname) {
        throw QueryParseError("field name for \"since\" term must be a string");
      }

      for (i = 0; i < sizeof(allowed_fields) / sizeof(allowed_fields[0]); ++i) {
        if (!strcmp(allowed_fields[i].label, fieldname)) {
          selected_field = allowed_fields[i].value;
          valid = true;
          break;
        }
      }

      if (!valid) {
        throw QueryParseError(
            "invalid field name \"", fieldname, "\" for \"since\" term");
      }
    }

    switch (selected_field) {
      case since_what::SINCE_CTIME:
      case since_what::SINCE_MTIME:
        if (!std::holds_alternative<ClockSpec::Timestamp>(spec->spec)) {
          throw QueryParseError(
              "field \"",
              fieldname,
              "\" requires a timestamp value for comparison in \"since\" term");
        }
        break;
      case since_what::SINCE_OCLOCK:
      case since_what::SINCE_CCLOCK:
        /* we'll work with clocks or timestamps */
        break;
    }

    return std::make_unique<SinceExpr>(std::move(spec), selected_field);
  }
};
W_TERM_PARSER(since, SinceExpr::parse);

/* vim:ts=2:sw=2:et:
 */
