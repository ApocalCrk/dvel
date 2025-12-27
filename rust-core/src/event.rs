// Event definition: fixed layout, deterministic canonical form; identity = SHA256(canonical || sig)
use crate::PROTOCOL_VERSION;

pub type Hash = [u8; 32];
pub type PublicKey = [u8; 32];
pub type Signature = [u8; 64];

/// Genesis marker (no parent).
pub const ZERO_HASH: Hash = [0u8; 32];

#[derive(Debug, Clone)]
pub struct Event {
    pub version: u8,
    pub prev_hash: Hash,
    pub author: PublicKey,
    pub timestamp: u64,
    pub payload_hash: Hash,
    pub signature: Signature,
}

impl Event {
    /// Construct from trusted local inputs; validity remains a separate check.
    pub fn new(
        prev_hash: Hash,
        author: PublicKey,
        timestamp: u64,
        payload_hash: Hash,
        signature: Signature,
    ) -> Self {
        Event {
            version: PROTOCOL_VERSION,
            prev_hash,
            author,
            timestamp,
            payload_hash,
            signature,
        }
    }

    /// Construct from raw fields (e.g., network/replay); no validation performed.
    pub fn from_raw(
        version: u8,
        prev_hash: Hash,
        author: PublicKey,
        timestamp: u64,
        payload_hash: Hash,
        signature: Signature,
    ) -> Self {
        Event {
            version,
            prev_hash,
            author,
            timestamp,
            payload_hash,
            signature,
        }
    }

    /// Canonical byte encoding (hash/sign input).
    /// Layout: [version (1)] [prev_hash (32)] [author (32)] [timestamp (8 LE)] [payload_hash (32)].
    pub fn canonical_bytes(&self) -> Vec<u8> {
        let mut out = Vec::with_capacity(1 + 32 + 32 + 8 + 32);
        out.push(self.version);
        out.extend_from_slice(&self.prev_hash);
        out.extend_from_slice(&self.author);
        out.extend_from_slice(&self.timestamp.to_le_bytes());
        out.extend_from_slice(&self.payload_hash);
        out
    }

    /// Identity material: canonical bytes concatenated with signature.
    pub fn hash_material(&self) -> Vec<u8> {
        let mut out = self.canonical_bytes();
        out.extend_from_slice(&self.signature);
        out
    }
}
