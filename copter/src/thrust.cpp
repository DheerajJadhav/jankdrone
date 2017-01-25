#include <Arduino.h>
#include <cmath>
#include <EEPROM.h>

#include "config.h"
#include "shm.h"
#include "log.h"
#include "thrust.h"

Thrust::Thruster::Thruster(int pin, Shm::Var* thrustValue): m_thrustValue{thrustValue} {
	m_esc.attach(pin);
	(*this)(0);
}

void Thrust::Thruster::operator()() {
	(*this)(m_thrustValue->getFloat());
}

void Thrust::Thruster::operator()(float thrustValue) {
	static auto calibrate = Shm::var("switches.calibrate");
	if (calibrate->getBool()) {
		EEPROM.write(CALIBRATED_ADDRESS, false);
		Log::fatal("Shutting down to calibrate");
	}

	static auto softKill = Shm::var("switches.softKill");
	if (softKill->getBool()) {
		thrustNoKillCheck(0);
	} else {
		thrustNoKillCheck(thrustValue);
	}
}

void Thrust::Thruster::thrustNoKillCheck(float thrustValue) {
	float clampedThrust = std::max(0.0, std::min(1.0, thrustValue));
	float dutyCycleMicros = MIN_THRUSTER_PULSE + 
		(MAX_THRUSTER_PULSE - MIN_THRUSTER_PULSE) * clampedThrust;
	int roundedMicros = (int)round(dutyCycleMicros);
	m_esc.writeMicroseconds(roundedMicros);
}

Thrust::Thrust(): m_thrusters{
		Thruster(RIGHT_THRUSTER_PIN, Shm::var("thrusters.right")),
		Thruster(FRONT_RIGHT_THRUSTER_PIN, Shm::var("thrusters.frontRight")),
		Thruster(FRONT_LEFT_THRUSTER_PIN, Shm::var("thrusters.frontLeft")),
		Thruster(LEFT_THRUSTER_PIN, Shm::var("thrusters.left")),
		Thruster(BACK_LEFT_THRUSTER_PIN, Shm::var("thrusters.backLeft")),
		Thruster(BACK_RIGHT_THRUSTER_PIN, Shm::var("thrusters.backRight")),
} {
	if (!EEPROM.read(CALIBRATED_ADDRESS)) {
		EEPROM.write(CALIBRATED_ADDRESS, true); // Write early in case we're interrupted

		static auto softKill = Shm::var("switches.softKill");
		bool lastSoftKill = softKill->getBool();
		softKill->set(false);

		Log::info("Calibrating thrusters...");
		for (auto& t : m_thrusters) t(1);
		delay(2500); // Wait for ESC to power on and register first input
		for (auto& t : m_thrusters) t(0);
		delay(500); // Wait for ESC to register second input
		Log::info("Done calibrating thrusters");

		softKill->set(lastSoftKill);
	}
}

void Thrust::operator()() {
	for (auto& t : m_thrusters) t();
}
