#include "MusclePlugin.hpp"

namespace roboy_simulation {

	double PIDcontroller::calc_output(double cmd, double pos, double timePeriod) {
		float pterm, dterm, result, err, ffterm;
		if (cmd >= spNegMax && cmd <= spPosMax) {
			err = pos - cmd;
			if ((err > params.pidParameters.deadBand) || (err < -1 * params.pidParameters.deadBand)) {
				pterm = params.pidParameters.pgain * err;
				if ((pterm < outputPosMax) || (pterm > outputNegMax)) //if the proportional term is not maxed
				{
					integral += (params.pidParameters.igain * err * (timePeriod)); //add to the integral
					if (integral > params.pidParameters.IntegralPosMax)
						integral = params.pidParameters.IntegralPosMax;
					else if (integral < params.pidParameters.IntegralNegMax)
						integral = params.pidParameters.IntegralNegMax;
				}

				dterm = ((err - lastError) / timePeriod) * params.pidParameters.dgain;

				ffterm = params.pidParameters.forwardGain * cmd;
				result = ffterm + pterm + integral + dterm;
				if (result < outputNegMax)
					result = outputNegMax;
				else if (result > outputPosMax)
					result = outputPosMax;
			}
			else
				result = integral;
			lastError = err;
		} else {
			result = 0;
		}
		return result;
	}

	double ITendon::ElectricMotorModel(const double _current, const double _torqueConstant,
									   const double _spindleRadius) {
		double motorForce;

		if (_current >= 0) {
			motorForce = _current * _torqueConstant / _spindleRadius;
		}
		else {
			motorForce = 0;
		}

		return motorForce;
	}


	double ITendon::ElasticElementModel(const double _speed, const double _spindleRadius, const double _time) {
		// double realTimeUpdateRate=1000;
		double windingLength = _spindleRadius * _speed * _time;
		double displacement;
		displacement = windingLength + see.length - see.lengthRest;

		// gzdbg << "displacement: "
		// 	  << displacement
		// 	  << "\n"
		//          << "windingLength: "
		// 	  << windingLength
		// 	  << "\n";

		double elasticForce;

		if (displacement >= 0) {
			elasticForce = displacement * see.stiffness;
		}
		else {
			elasticForce = 0;
		}

		//return _stiffness[0] + (displacement*_stiffness[1]) + (displacement*displacement*_stiffness[2]) +
		//			(displacement*displacement*displacement*_stiffness[3]) ;
		//return displacement*_stiffness[0];
		return elasticForce;

	}

	math::Vector3 ITendon::CalculateForce(double _elasticForce, double _motorForce,
										  const math::Vector3 &_tendonOrien) {
		// math::Vector3 diff = _fixationP - _instertionP;

		/*    double tendonForce;

		if (_elasticForce+_motorForce>=0)
		{
			tendonForce=_elasticForce+_motorForce;
		}
		else
		{
			tendonForce=0;
		}*/

		return _tendonOrien * (_elasticForce + _motorForce);

	}

	void ITendon::GetTendonInfo(vector<math::Vector3> &viaPointPos, tendonType *tendon_p)//try later with pointer
	{
        see.length = 0;
		for (int i = 0; i < viaPointPos.size() - 1; i++) {
			tendon_p->MidPoint.push_back((viaPointPos[i] + viaPointPos[i + 1]) / 2);
			tendon_p->Vector.push_back(viaPointPos[i] - viaPointPos[i + 1]);
            double length = tendon_p->Vector[i].GetLength();
			tendon_p->Orientation.push_back(tendon_p->Vector[i] / length);
			tendon_p->Pitch.push_back(atan(tendon_p->Orientation[i][0] / tendon_p->Orientation[i][2]));
			tendon_p->Roll.push_back(
					-acos(sqrt((pow(tendon_p->Orientation[i][0], 2) + pow(tendon_p->Orientation[i][2], 2)))));
            see.length += length;
		}
	}

	double ITendon::DotProduct(const math::Vector3 &_v1, const math::Vector3 &_v2) {
		return _v1.x * _v2.x + _v1.y * _v2.y + _v1.z * _v2.z;
	}


	double ITendon::Angle(const math::Vector3 &_v1, const math::Vector3 &_v2) {
		return acos(_v1.Dot(_v2) / _v1.GetLength() * _v2.GetLength());
	}

	double IActuator::EfficiencyApproximation() {
		double param1 = 0.1; // defines steepness of the approximation
		double param2 = 0.0; // defines zero crossing of the approximation
		return gear.efficiency + (1 / gear.efficiency - gear.efficiency) *
								 (0.5 * (tanh(-param1 * spindle.angVel * motor.current - param2) + 1));
	}

	MusclePlugin::MusclePlugin() {
		x.resize(2);
	}

	void MusclePlugin::Init(MyoMuscleInfo &myoMuscle) {
		//state initialization
		x[0] = 0.0;
		x[1] = 0.0;
		actuator.motor.voltage = 0.0;
		actuator.spindle.angVel = 0;

        link_index = myoMuscle.link_index;
        links = myoMuscle.links;
        joint = myoMuscle.joint;
		viaPoints = myoMuscle.viaPoints;
        viaPointsInGlobalFrame = myoMuscle.viaPoints;
        force = myoMuscle.viaPoints;
		actuator.motor = myoMuscle.motor;
		actuator.gear = myoMuscle.gear;
		actuator.spindle = myoMuscle.spindle;
		tendon.see = myoMuscle.see;
        name = myoMuscle.name;

		pid.params.pidParameters.pgain = 1000;
		pid.params.pidParameters.igain = 0;
		pid.params.pidParameters.dgain = 0;
	}


	void MusclePlugin::Update( ros::Time &time, ros::Duration &period ) {
		// TODO: calculate PID result
//		pid.calc_output(cmd,pos,period);
		actuator.motor.voltage = cmd;

		tendonType newTendon;

        uint j = 0;
        for (uint i = 0; i < viaPointsInGlobalFrame.size(); i++) {
            // absolute position + relative position=actual position of each via point
            gazebo::math::Pose linkPose = links[j]->GetWorldPose();
            viaPointsInGlobalFrame[i] = linkPose.pos + linkPose.rot.RotateVector(viaPoints[i]);
            if(i==link_index-1){
                momentArm = (viaPointsInGlobalFrame[i]-joint->GetWorldPose().pos).Cross(
                        (viaPointsInGlobalFrame[i+1]-viaPointsInGlobalFrame[i]).Normalize());
                j++;
            }
		}

        tendon.GetTendonInfo(viaPointsInGlobalFrame, &newTendon);

		// calculate elastic force
		actuator.elasticForce = tendon.ElasticElementModel(actuator.spindle.angVel, actuator.spindle.radius, period.toSec());

		// calculate motor force
		// calculate the approximation of gear's efficiency
		actuator.gear.appEfficiency = actuator.EfficiencyApproximation();

		// do 1 step of integration of DiffModel() at current time
		actuator.stepper.do_step([this](const IActuator::state_type &x, IActuator::state_type &dxdt, const double) {
			// This lambda function describes the differential model for the simulations of dynamics
			// of a DC motor, a spindle, and a gear box
			// x[0] - motor electric current
			// x[1] - spindle angular velocity
			double totalIM = actuator.motor.inertiaMoment + actuator.gear.inertiaMoment; // total moment of inertia
			dxdt[0] = 1 / actuator.motor.inductance * (-actuator.motor.resistance * x[0] -
													   actuator.motor.BEMFConst * actuator.gear.ratio * x[1] +
													   actuator.motor.voltage);
			dxdt[1] = actuator.motor.torqueConst * x[0] / (actuator.gear.ratio * totalIM) -
					  actuator.spindle.radius * actuator.elasticForce /
					  (actuator.gear.ratio * actuator.gear.ratio * totalIM * actuator.gear.appEfficiency);
		}, x, time.toSec(), period.toSec());

		actuator.motor.current = x[0];
		actuator.spindle.angVel = x[1];

		actuatorForce = tendon.ElectricMotorModel(actuator.motor.current, actuator.motor.torqueConst,
												  actuator.spindle.radius);

        ROS_INFO_THROTTLE(1,"\ncurrent: %f\n"
                "voltage: %f\n"
                "spindle.angVel: %f\n"
                "actuatorForce %f\n"
                "tendon see length: %f\n"
                "t: %f\n"
                "dt: %f\n"
                "actuator.motor.inertiaMoment: %f\n"
                "actuator.gear.inertiaMoment: %f\n"
                "actuator.gear.ratio: %f\n"
                "actuator.motor.inductance: %f\n"
                "actuator.motor.resistance: %f\n"
                "actuator.elasticForce: %f\n"
                "actuator.gear.appEfficiency: %f\n"
                "actuator.motor.BEMFConst: %f\n"
                "actuator.motor.torqueConst: %f\n"
                "", actuator.motor.current, actuator.motor.voltage, actuator.spindle
                .angVel, actuatorForce,
                          tendon.see.length,  time.toSec(), period.toSec(), actuator.motor.inertiaMoment, actuator.gear
                                  .inertiaMoment, actuator.gear.ratio, actuator.motor.inductance, actuator.motor
                                  .resistance, actuator.elasticForce, actuator.gear
                                  .appEfficiency,actuator.motor.BEMFConst, actuator.motor.torqueConst);

		// calculate general force (elastic+actuator)
        for (uint i = 0; i < viaPointsInGlobalFrame.size(); i++) {
            force[i] = tendon.CalculateForce(actuator.elasticForce, actuatorForce, newTendon.Orientation[i]);
        }
	}
}
// make it a plugin loadable via pluginlib
PLUGINLIB_EXPORT_CLASS(roboy_simulation::MusclePlugin, roboy_simulation::MusclePlugin)