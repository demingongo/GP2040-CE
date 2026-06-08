import { useTranslation } from 'react-i18next';
import { FormCheck, Row } from 'react-bootstrap';

import Section from '../Components/Section';
import FormControl from '../Components/FormControl';
import { AddonPropTypes } from '../Pages/AddonsConfigPage';

export const espUartBridgeScheme = {};

export const espUartBridgeState = {
	EspUartBridgeAddonEnabled: 0,
	espUartBridgeUartBlock: 1,
	espUartBridgeTxPin: 20,
	espUartBridgeRxPin: 21,
	espUartBridgeBaudRate: 1000000,
	espUartBridgeDisableWhenUsbConnected: 1,
};

const EspUartBridge = ({ values, errors, handleChange, handleCheckbox }: AddonPropTypes) => {
	const { t } = useTranslation();
	return (
		<Section title={t('AddonsConfig:esp-uart-bridge-header-text')}>
			<div id="EspUartBridgeOptions" hidden={!values.EspUartBridgeAddonEnabled}>
				<Row className="mb-3">
					<FormControl
						type="number"
						label={t('AddonsConfig:esp-uart-bridge-uart-block-label')}
						name="espUartBridgeUartBlock"
						className="form-select-sm"
						groupClassName="col-sm-3 mb-3"
						value={values.espUartBridgeUartBlock}
						error={errors.espUartBridgeUartBlock}
						isInvalid={Boolean(errors.espUartBridgeUartBlock)}
						onChange={handleChange}
						min={0}
						max={1}
					/>
					<FormControl
						type="number"
						label={t('AddonsConfig:esp-uart-bridge-tx-pin-label')}
						name="espUartBridgeTxPin"
						className="form-select-sm"
						groupClassName="col-sm-3 mb-3"
						value={values.espUartBridgeTxPin}
						error={errors.espUartBridgeTxPin}
						isInvalid={Boolean(errors.espUartBridgeTxPin)}
						onChange={handleChange}
						min={-1}
						max={29}
					/>
					<FormControl
						type="number"
						label={t('AddonsConfig:esp-uart-bridge-rx-pin-label')}
						name="espUartBridgeRxPin"
						className="form-select-sm"
						groupClassName="col-sm-3 mb-3"
						value={values.espUartBridgeRxPin}
						error={errors.espUartBridgeRxPin}
						isInvalid={Boolean(errors.espUartBridgeRxPin)}
						onChange={handleChange}
						min={-1}
						max={29}
					/>
					<FormControl
						type="number"
						label={t('AddonsConfig:esp-uart-bridge-baud-rate-label')}
						name="espUartBridgeBaudRate"
						className="form-select-sm"
						groupClassName="col-sm-3 mb-3"
						value={values.espUartBridgeBaudRate}
						error={errors.espUartBridgeBaudRate}
						isInvalid={Boolean(errors.espUartBridgeBaudRate)}
						onChange={handleChange}
						min={9600}
						max={2000000}
					/>
				</Row>
				<FormCheck
					label={t('AddonsConfig:esp-uart-bridge-disable-when-usb-label')}
					type="switch"
					id="EspUartBridgeDisableWhenUsbConnected"
					reverse={true}
					isInvalid={false}
					checked={Boolean(values.espUartBridgeDisableWhenUsbConnected)}
					onChange={(e) => {
						handleCheckbox('espUartBridgeDisableWhenUsbConnected');
						handleChange(e);
					}}
				/>
			</div>
			<FormCheck
				label={t('Common:switch-enabled')}
				type="switch"
				id="EspUartBridgeButton"
				reverse={true}
				isInvalid={false}
				checked={Boolean(values.EspUartBridgeAddonEnabled)}
				onChange={(e) => {
					handleCheckbox('EspUartBridgeAddonEnabled');
					handleChange(e);
				}}
			/>
		</Section>
	);
};

export default EspUartBridge;
