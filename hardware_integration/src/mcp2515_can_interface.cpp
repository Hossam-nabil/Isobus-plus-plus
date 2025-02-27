//================================================================================================
/// @file mcp2515_can_interface.cpp
///
/// @brief An interface for using the MCP2515 can controller.
/// @author Daan Steenbergen
///
/// @copyright 2022 Adrian Del Grosso
//================================================================================================
#include "isobus/hardware_integration/mcp2515_can_interface.hpp"
#include "isobus/isobus/can_stack_logger.hpp"
#include "isobus/utility/system_timing.hpp"

#include <iostream>

MCP2515CANInterface::MCP2515CANInterface(SPIHardwarePlugin *transactionHandler, const std::uint8_t cfg1, const std::uint8_t cfg2, const std::uint8_t cfg3) :
  transactionHandler(transactionHandler),
  cfg1(cfg1),
  cfg2(cfg2),
  cfg3(cfg3)
{
}

MCP2515CANInterface::~MCP2515CANInterface()
{
	close();
}

bool MCP2515CANInterface::get_is_valid() const
{
	return (transactionHandler) && (initialized);
}

void MCP2515CANInterface::close()
{
	initialized = false;
}

void MCP2515CANInterface::open()
{
	if (reset())
	{
		if (set_mode(MCPMode::CONFIG))
		{
			if ((write_register(MCPRegister::CNF1, cfg1)) &&
			    (write_register(MCPRegister::CNF2, cfg2)) &&
			    (write_register(MCPRegister::CNF3, cfg3)))
			{
				if (set_mode(MCPMode::NORMAL))
				{
					initialized = true;
				}
			}
		}
	}
}

bool MCP2515CANInterface::reset()
{
	bool retVal = false;
	if (write_reset())
	{
		// Depends on oscillator & capacitors used
		std::this_thread::sleep_for(std::chrono::milliseconds(10));

		std::uint8_t zeros[14] = { 0 };
		if ((write_register(MCPRegister::TXB0CTRL, zeros, 14)) &&
		    (write_register(MCPRegister::TXB1CTRL, zeros, 14)) &&
		    (write_register(MCPRegister::TXB2CTRL, zeros, 14)) &&
		    (write_register(MCPRegister::RXB0CTRL, 0)) &&
		    (write_register(MCPRegister::RXB1CTRL, 0)) &&
		    (write_register(MCPRegister::CANINTE, 0xA3)))
		{
			retVal = true;
		}
	}
	return retVal;
}

bool MCP2515CANInterface::get_read_status(std::uint8_t &status)
{
	bool retVal = false;
	if (transactionHandler)
	{
		const std::vector<std::uint8_t> txBuffer = { static_cast<std::uint8_t>(MCPInstruction::READ_STATUS), 0x00 };
		SPITransactionFrame frame(&txBuffer, true);
		transactionHandler->begin_transaction();
		transactionHandler->transmit(&frame);
		if (transactionHandler->end_transaction())
		{
			retVal = true;
			frame.read_byte(1, status);
		}
	}
	return retVal;
}
bool MCP2515CANInterface::read_register(const MCPRegister address, std::uint8_t &data)
{
	bool retVal = false;
	if (transactionHandler)
	{
		const std::vector<std::uint8_t> txBuffer = { static_cast<std::uint8_t>(MCPInstruction::READ),
			                                           static_cast<std::uint8_t>(address),
			                                           0x00 };
		SPITransactionFrame frame(&txBuffer, true);

		transactionHandler->begin_transaction();
		transactionHandler->transmit(&frame);
		if (transactionHandler->end_transaction())
		{
			retVal = true;
			frame.read_byte(2, data);
		}
	}
	return retVal;
}

bool MCP2515CANInterface::read_register(const MCPRegister address, std::uint8_t *data, const std::size_t length)
{
	bool retVal = false;
	if (transactionHandler)
	{
		std::vector<std::uint8_t> txBuffer(2 + length, 0x00);
		txBuffer[0] = static_cast<std::uint8_t>(MCPInstruction::READ);
		txBuffer[1] = static_cast<std::uint8_t>(address);
		SPITransactionFrame frame(&txBuffer, true);
		transactionHandler->begin_transaction();
		transactionHandler->transmit(&frame);
		if (transactionHandler->end_transaction())
		{
			retVal = true;
			frame.read_bytes(2, data, length);
		}
	}
	return retVal;
}

bool MCP2515CANInterface::modify_register(const MCPRegister address, const std::uint8_t mask, const std::uint8_t data)
{
	bool retVal = false;
	if (transactionHandler)
	{
		const std::vector<std::uint8_t> txBuffer = { static_cast<std::uint8_t>(MCPInstruction::BITMOD),
			                                           static_cast<std::uint8_t>(address),
			                                           mask,
			                                           data };
		SPITransactionFrame frame(&txBuffer);
		transactionHandler->begin_transaction();
		transactionHandler->transmit(&frame);
		retVal = transactionHandler->end_transaction();
	}
	return retVal;
}

bool MCP2515CANInterface::write_reset()
{
	bool retVal = false;
	if (transactionHandler)
	{
		const std::vector<std::uint8_t> txBuffer = { static_cast<std::uint8_t>(MCPInstruction::RESET) };
		SPITransactionFrame frame(&txBuffer);
		transactionHandler->begin_transaction();
		transactionHandler->transmit(&frame);
		retVal = transactionHandler->end_transaction();
	}
	return retVal;
}

bool MCP2515CANInterface::write_register(const MCPRegister address, const std::uint8_t data)
{
	bool retVal = false;
	if (transactionHandler)
	{
		const std::vector<std::uint8_t> txBuffer = { static_cast<std::uint8_t>(MCPInstruction::WRITE),
			                                           static_cast<std::uint8_t>(address),
			                                           data };
		SPITransactionFrame frame(&txBuffer);
		transactionHandler->begin_transaction();
		transactionHandler->transmit(&frame);
		retVal = transactionHandler->end_transaction();
	}
	return retVal;
}

bool MCP2515CANInterface::write_register(const MCPRegister address, const std::uint8_t data[], const std::size_t length)
{
	bool retVal = false;
	if (transactionHandler)
	{
		std::vector<std::uint8_t> txBuffer(2 + length, 0x00);
		txBuffer[0] = static_cast<std::uint8_t>(MCPInstruction::WRITE);
		txBuffer[1] = static_cast<std::uint8_t>(address);
		memcpy(txBuffer.data() + 2, data, length);
		SPITransactionFrame frame(&txBuffer);

		transactionHandler->begin_transaction();
		transactionHandler->transmit(&frame);
		retVal = transactionHandler->end_transaction();
	}
	return retVal;
}

bool MCP2515CANInterface::set_mode(const MCPMode mode)
{
	bool retVal = false;
	if (modify_register(MCPRegister::CANCTRL, 0xE0, static_cast<std::uint8_t>(mode)))
	{
		// Wait for the mode to be set within 10ms
		const auto start = isobus::SystemTiming::get_timestamp_ms();
		while (isobus::SystemTiming::get_time_elapsed_ms(start) < 10)
		{
			std::uint8_t newMode;
			if (read_register(MCPRegister::CANSTAT, newMode))
			{
				if ((newMode & 0xE0) == static_cast<std::uint8_t>(mode))
				{
					retVal = true;
					break;
				}
			}
		}
	}
	return retVal;
}
bool MCP2515CANInterface::read_frame(isobus::HardwareInterfaceCANFrame &canFrame,
                                     const MCPRegister ctrlRegister,
                                     const MCPRegister dataRegister,
                                     const std::uint8_t intfMask)
{
	bool retVal = false;

	std::uint8_t buffer[6];
	if (read_register(ctrlRegister, buffer, 6))
	{
		canFrame.identifier = (buffer[1] << 3) + (buffer[2] >> 5);

		if (0x08 == (buffer[2] & 0x08))
		{
			canFrame.identifier = (canFrame.identifier << 2) + (buffer[2] & 0x03);
			canFrame.identifier = (canFrame.identifier << 8) + buffer[3];
			canFrame.identifier = (canFrame.identifier << 8) + buffer[4];
			canFrame.isExtendedFrame = true;
		}

		std::uint8_t ctrl = buffer[0];
		if (ctrl & 0x08)
		{
			// TODO: Handle remote frames
		}

		canFrame.dataLength = (buffer[5] & 0x0F);
		if (isobus::CAN_DATA_LENGTH >= canFrame.dataLength)
		{
			if ((read_register(dataRegister, canFrame.data, canFrame.dataLength)) &&
			    (modify_register(MCPRegister::CANINTF, intfMask, 0)))
			{
				retVal = true;
			}
		}
	}
	return retVal;
}

bool MCP2515CANInterface::read_frame(isobus::HardwareInterfaceCANFrame &canFrame)
{
	bool retVal = false;

	// TODO: implement interrupt driven message reading
	std::uint8_t status;
	while (get_read_status(status))
	{
		// Check if any of the buffers have a message
		if (status & 0x01)
		{
			retVal = read_frame(canFrame, MCPRegister::RXB0CTRL, MCPRegister::RXB0DATA, 0x01);
			break;
		}
		else if (status & 0x02)
		{
			retVal = read_frame(canFrame, MCPRegister::RXB1CTRL, MCPRegister::RXB1DATA, 0x02);
			break;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(RECEIVE_MESSAGE_READ_RATE));
	}

	return retVal;
}

bool MCP2515CANInterface::write_frame(const isobus::HardwareInterfaceCANFrame &canFrame,
                                      const MCPRegister ctrlRegister,
                                      const MCPRegister sidhRegister)
{
	bool retVal = false;

	// Check if the write buffer is empty
	std::uint8_t ctrl;
	if (read_register(ctrlRegister, ctrl))
	{
		if ((ctrl & 0x08) == 0)
		{
			// Buffer is empty now we can write the buffer
			std::uint8_t buffer[13];
			if (canFrame.isExtendedFrame)
			{
				buffer[3] = static_cast<std::uint8_t>(canFrame.identifier & 0xFF); //< EID0
				buffer[2] = static_cast<std::uint8_t>((canFrame.identifier >> 8) & 0xFF); //< EID8
				buffer[1] = static_cast<std::uint8_t>((canFrame.identifier >> 16) & 0x03); //< SIDL EID17-16
				buffer[1] |= static_cast<std::uint8_t>((canFrame.identifier >> 13) & 0xE0); //< SIDL SID0-2
				buffer[1] |= 0x08; //< SIDL exide mask
				buffer[0] = static_cast<std::uint8_t>((canFrame.identifier >> 21) & 0xFF); //< SIDH
			}
			else
			{
				buffer[1] = static_cast<std::uint8_t>((canFrame.identifier << 5) & 0xE0); // SIDL
				buffer[0] = static_cast<std::uint8_t>(canFrame.identifier >> 3); // SIDH
			}

			buffer[4] = canFrame.dataLength; //< DLC
			memcpy(&buffer[5], canFrame.data, canFrame.dataLength); //< Data
			if (write_register(sidhRegister, buffer, 5 + canFrame.dataLength))
			{
				// Now indicate that the buffer is ready to be sent
				if (modify_register(ctrlRegister, 0x08, 0x08))
				{
					// Now check if the buffer was sent
					if (read_register(ctrlRegister, ctrl))
					{
						if (0 == (ctrl & (0x40 | 0x20 | 0x10)))
						{
							retVal = true;
						}
					}
				}
			}
		}
	}

	return retVal;
}

bool MCP2515CANInterface::write_frame(const isobus::HardwareInterfaceCANFrame &canFrame)
{
	bool retVal = write_frame(canFrame, MCPRegister::TXB0CTRL, MCPRegister::TXB0SIDH);
	if (!retVal)
	{
		retVal = write_frame(canFrame, MCPRegister::TXB1CTRL, MCPRegister::TXB1SIDH);
	}
	if (!retVal)
	{
		retVal = write_frame(canFrame, MCPRegister::TXB2CTRL, MCPRegister::TXB2SIDH);
	}
	return retVal;
}
