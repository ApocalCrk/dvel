//! Deterministic trace checker (non-ZK) for merged_trace.json artifacts.
use crate::event::{Event, Hash, PublicKey, Signature};
use crate::ledger::{Ledger, ZERO_HASH};
use crate::scoring::{SybilConfig, SybilOverlay};
use crate::validation::{ValidationContext, validate_event};
use serde::Deserialize;

#[derive(Debug, Deserialize)]
pub struct TraceHeader {
    pub protocol_version: u8,
    pub max_backward_skew: u64,
    pub max_pending_total: u64,
    pub max_drain_steps: u64,
    pub sybil_config: SybilConfigSerde,
    pub final_merkle_root: Option<String>,
    pub sources: Vec<String>,
}

#[derive(Debug, Deserialize)]
pub struct SybilConfigSerde {
    pub warmup_ticks: u64,
    pub quarantine_ticks: u64,
    pub fixed_point_scale: u64,
    pub max_link_walk: usize,
}

impl From<SybilConfigSerde> for SybilConfig {
    fn from(s: SybilConfigSerde) -> Self {
        SybilConfig {
            warmup_ticks: s.warmup_ticks,
            quarantine_ticks: s.quarantine_ticks,
            policy: crate::scoring::EquivocationPolicy::Quarantine,
            fixed_point_scale: s.fixed_point_scale,
            max_link_walk: s.max_link_walk,
        }
    }
}

#[derive(Debug, Deserialize)]
pub struct TraceRowSerde {
    pub node_id: u32,
    pub row_index: usize,
    pub prev_hash: String,
    pub author: String,
    pub timestamp: u64,
    pub payload_hash: String,
    pub signature: String,
    pub parent_present: bool,
    pub ancestor_check: bool,
    pub quarantined_until_before: u64,
    pub quarantined_until_after: u64,
    pub merkle_root: Option<String>,
    pub merkle_root_has: bool,
    pub preferred_tip: Option<String>,
    pub preferred_tip_has: bool,
    pub author_weight_fp: u64,
}

#[derive(Debug, Deserialize)]
pub struct TraceDoc {
    pub header: TraceHeader,
    pub rows: Vec<TraceRowSerde>,
}

fn hex32(s: &str) -> Option<Hash> {
    if s.len() != 64 {
        return None;
    }
    let mut out = [0u8; 32];
    for i in 0..32 {
        let byte = u8::from_str_radix(&s[2 * i..2 * i + 2], 16).ok()?;
        out[i] = byte;
    }
    Some(out)
}

fn hex64(s: &str) -> Option<Signature> {
    if s.len() != 128 {
        return None;
    }
    let mut out = [0u8; 64];
    for i in 0..64 {
        let byte = u8::from_str_radix(&s[2 * i..2 * i + 2], 16).ok()?;
        out[i] = byte;
    }
    Some(out)
}

fn parse_row(row: &TraceRowSerde) -> Option<Event> {
    Some(Event::from_raw(
        crate::PROTOCOL_VERSION,
        hex32(&row.prev_hash)?,
        hex32(&row.author)?,
        row.timestamp,
        hex32(&row.payload_hash)?,
        hex64(&row.signature)?,
    ))
}

/// Checks the merged trace deterministically. Returns Ok(()) if all invariants hold.
pub fn check_trace(doc: TraceDoc) -> Result<(), String> {
    let cfg: SybilConfig = doc.header.sybil_config.into();
    let mut overlay = SybilOverlay::new(cfg.clone());
    let mut ledger = Ledger::new();
    let mut vctxs: std::collections::HashMap<PublicKey, ValidationContext> =
        std::collections::HashMap::new();

    let mut last_root: Option<Hash> = None;

    for (idx, r) in doc.rows.iter().enumerate() {
        let ev = parse_row(r).ok_or_else(|| format!("row {} parse error", idx))?;

        // parent_present check
        let parent_is_zero = ev.prev_hash == ZERO_HASH;
        let parent_known = ledger.get_event(&ev.prev_hash).is_some();
        if !parent_is_zero && !parent_known && r.parent_present {
            return Err(format!(
                "row {} parent_present=true but parent unknown",
                idx
            ));
        }
        if !parent_is_zero && parent_known && !r.parent_present {
            return Err(format!("row {} parent_present=false but parent known", idx));
        }

        // Validate signature/timestamp
        let ctx = vctxs
            .entry(ev.author)
            .or_insert_with(ValidationContext::new);
        validate_event(&ev, ctx).map_err(|e| format!("row {} validate error {:?}", idx, e))?;

        // Link
        let h = Ledger::hash_event(&ev);
        ledger
            .try_add_event(ev.clone())
            .map_err(|e| format!("row {} link error {:?}", idx, e))?;

        // Overlay observe
        overlay.observe_event(&ledger, r.timestamp, r.node_id, &ev, h);

        // Quarantine window monotone
        if !r.ancestor_check
            && r.quarantined_until_after < r.quarantined_until_before + cfg.quarantine_ticks
        {
            return Err(format!(
                "row {} quarantine_after too small: before {} after {}",
                idx, r.quarantined_until_before, r.quarantined_until_after
            ));
        }

        // Weight bounds
        if r.author_weight_fp > cfg.fixed_point_scale {
            return Err(format!("row {} author_weight_fp out of bounds", idx));
        }
        if r.timestamp < r.quarantined_until_after && r.author_weight_fp != 0 {
            return Err(format!("row {} weight not zero during quarantine", idx));
        }

        // Merkle root
        if let Some(root) = ledger.merkle_root() {
            last_root = Some(root);
            if let Some(mr_str) = &r.merkle_root {
                if let Some(mr_row) = hex32(mr_str) {
                    if mr_row != root {
                        return Err(format!("row {} merkle_root mismatch", idx));
                    }
                }
            }
        }
    }

    if let (Some(hdr_root), Some(last)) = (&doc.header.final_merkle_root, last_root) {
        if let Some(expected) = hex32(hdr_root) {
            if expected != last {
                return Err("final_merkle_root mismatch".into());
            }
        }
    }

    Ok(())
}
