#pragma once
#include "esp_log.h"

#include "JsonWrapper.h"

inline double rr(double value) {
      return (int)(value * 100.0 + 0.5) / 100.0;
}



struct Value {
  virtual const std::string etype() const { return "und"; }
  virtual void print() const { ESP_LOGE("Events::Value", "un-overridden '%s'", etype().c_str()); }
  virtual std::unique_ptr<Value> clone() const = 0;
  virtual bool isEqual(const Value& other) const = 0;
  virtual float get_main() const { return 0.0; }
  virtual float get_power() const { return 0.0; }

  virtual JsonWrapper& toJson(JsonWrapper& doc) const {
    doc.AddItem("type", etype());
    return doc;
  }
};

struct Range : public Value {
  float x = 0.0;
  float y = 0.0;
  float speed;
  int reference;

  const std::string  etype() const override { return "rng"; }

  Range(float x, float y, float speed, int reference=0) : x(x), y(y), speed(speed), reference(reference) {}
  virtual float get_main() const { return speed; }

  virtual void print() const override {
    ESP_LOGI("Events", "Range: speed %1.2f x pos %1.2f Y pos %1.2f %2d", speed, x, y, reference);
  }

  std::unique_ptr<Value> clone() const override {
    return std::unique_ptr<Value>(new Range(*this));
  }

  bool isEqual(const Value& other) const override {
    const Range& o = static_cast<const Range&>(other);
    return x == o.x && y == o.y && speed == o.speed && reference == o.reference;
  }

  JsonWrapper& toJson(JsonWrapper& doc) const override {
    Value::toJson(doc);
    doc.AddItem("x", rr(x));
    doc.AddItem("y", rr(y));
    doc.AddItem("speed", rr(speed));
    doc.AddItem("reference", reference);
    return doc;
  }
};

struct NoTarget : public Value {
  const std::string etype() const override { return "no"; }

  virtual void print() const override {
    ESP_LOGI("Events", "no target");
  }

  std::unique_ptr<Value> clone() const override {
    return std::unique_ptr<Value>(new NoTarget(*this));
  }

  bool isEqual(const Value& other) const override {
    return true;
  }
};


class EventProc {
public:
  virtual void Detected(Value *vv) = 0;
  virtual void Cleared() = 0;
  virtual void TrackingUpdate(Value *cc) = 0;
  virtual void PresenceUpdate(Value *cc) = 0;
};

