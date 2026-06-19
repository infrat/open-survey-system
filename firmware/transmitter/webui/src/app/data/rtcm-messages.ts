export interface RTCMMessageType {
  id: number;
  name: string;
  description: string;
  category: 'GPS' | 'GLONASS' | 'Galileo' | 'BeiDou' | 'Multi-GNSS' | 'Station' | 'Other';
}

export const RTCM_MESSAGE_TYPES: RTCMMessageType[] = [
  // Station Info
  { id: 1005, name: 'Stationary RTK Reference Station ARP', description: 'Base station antenna reference point coordinates', category: 'Station' },
  { id: 1006, name: 'Stationary RTK Reference Station ARP with Height', description: 'Base station coordinates with antenna height', category: 'Station' },
  { id: 1007, name: 'Antenna Descriptor', description: 'Antenna serial number and descriptor', category: 'Station' },
  { id: 1008, name: 'Antenna Descriptor & Serial Number', description: 'Extended antenna information', category: 'Station' },
  { id: 1033, name: 'Receiver and Antenna Descriptors', description: 'Combined receiver and antenna information', category: 'Station' },

  // GPS
  { id: 1001, name: 'GPS L1 Code & Phase', description: 'GPS L1-only RTK observables', category: 'GPS' },
  { id: 1002, name: 'GPS L1 Extended Code & Phase', description: 'GPS L1 with extended information', category: 'GPS' },
  { id: 1003, name: 'GPS L1/L2 Code & Phase', description: 'GPS dual-frequency RTK observables', category: 'GPS' },
  { id: 1004, name: 'GPS L1/L2 Extended Code & Phase', description: 'GPS L1/L2 with extended information', category: 'GPS' },
  { id: 1019, name: 'GPS Ephemeris', description: 'GPS satellite orbit data', category: 'GPS' },

  // GLONASS
  { id: 1009, name: 'GLONASS L1 Code & Phase', description: 'GLONASS L1-only RTK observables', category: 'GLONASS' },
  { id: 1010, name: 'GLONASS L1 Extended Code & Phase', description: 'GLONASS L1 with extended information', category: 'GLONASS' },
  { id: 1011, name: 'GLONASS L1/L2 Code & Phase', description: 'GLONASS dual-frequency RTK observables', category: 'GLONASS' },
  { id: 1012, name: 'GLONASS L1/L2 Extended Code & Phase', description: 'GLONASS L1/L2 with extended information', category: 'GLONASS' },
  { id: 1020, name: 'GLONASS Ephemeris', description: 'GLONASS satellite orbit data', category: 'GLONASS' },

  // Multi-GNSS (MSM)
  { id: 1074, name: 'GPS MSM4', description: 'GPS Multi-Signal Message (compact)', category: 'Multi-GNSS' },
  { id: 1075, name: 'GPS MSM5', description: 'GPS Multi-Signal Message (full precision)', category: 'Multi-GNSS' },
  { id: 1076, name: 'GPS MSM6', description: 'GPS MSM with high-rate data', category: 'Multi-GNSS' },
  { id: 1077, name: 'GPS MSM7', description: 'GPS MSM with highest precision', category: 'Multi-GNSS' },

  { id: 1084, name: 'GLONASS MSM4', description: 'GLONASS Multi-Signal Message (compact)', category: 'Multi-GNSS' },
  { id: 1085, name: 'GLONASS MSM5', description: 'GLONASS Multi-Signal Message (full precision)', category: 'Multi-GNSS' },
  { id: 1086, name: 'GLONASS MSM6', description: 'GLONASS MSM with high-rate data', category: 'Multi-GNSS' },
  { id: 1087, name: 'GLONASS MSM7', description: 'GLONASS MSM with highest precision', category: 'Multi-GNSS' },

  { id: 1094, name: 'Galileo MSM4', description: 'Galileo Multi-Signal Message (compact)', category: 'Multi-GNSS' },
  { id: 1095, name: 'Galileo MSM5', description: 'Galileo Multi-Signal Message (full precision)', category: 'Multi-GNSS' },
  { id: 1096, name: 'Galileo MSM6', description: 'Galileo MSM with high-rate data', category: 'Multi-GNSS' },
  { id: 1097, name: 'Galileo MSM7', description: 'Galileo MSM with highest precision', category: 'Multi-GNSS' },

  { id: 1124, name: 'BeiDou MSM4', description: 'BeiDou Multi-Signal Message (compact)', category: 'Multi-GNSS' },
  { id: 1125, name: 'BeiDou MSM5', description: 'BeiDou Multi-Signal Message (full precision)', category: 'Multi-GNSS' },
  { id: 1126, name: 'BeiDou MSM6', description: 'BeiDou MSM with high-rate data', category: 'Multi-GNSS' },
  { id: 1127, name: 'BeiDou MSM7', description: 'BeiDou MSM with highest precision', category: 'Multi-GNSS' },

  // Galileo
  { id: 1045, name: 'Galileo F/NAV Ephemeris', description: 'Galileo satellite orbit data (F/NAV)', category: 'Galileo' },
  { id: 1046, name: 'Galileo I/NAV Ephemeris', description: 'Galileo satellite orbit data (I/NAV)', category: 'Galileo' },

  // Other
  { id: 1013, name: 'System Parameters', description: 'System-level parameters', category: 'Other' },
  { id: 1029, name: 'Unicode Text String', description: 'Text message in Unicode', category: 'Other' },
  { id: 1230, name: 'GLONASS Code-Phase Biases', description: 'GLONASS L1/L2 code-phase bias information', category: 'Other' },
];

export const CATEGORY_COLORS: Record<RTCMMessageType['category'], string> = {
  'GPS': 'bg-blue-500/10 text-blue-700 border-blue-200',
  'GLONASS': 'bg-red-500/10 text-red-700 border-red-200',
  'Galileo': 'bg-purple-500/10 text-purple-700 border-purple-200',
  'BeiDou': 'bg-yellow-500/10 text-yellow-700 border-yellow-200',
  'Multi-GNSS': 'bg-green-500/10 text-green-700 border-green-200',
  'Station': 'bg-gray-500/10 text-gray-700 border-gray-200',
  'Other': 'bg-orange-500/10 text-orange-700 border-orange-200',
};
